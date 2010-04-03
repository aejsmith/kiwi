/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		File map functions.
 *
 * The functions in this file implement a cache for file block numbers to a
 * raw (i.e. on-disk) block number. Also provided are VM cache helper functions
 * that can use data in a file map to handle reading and writing of data pages.
 */

#include <io/file_map.h>

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <assert.h>
#include <fatal.h>
#include <init.h>

/** Structure containing a single range in a file map. */
typedef struct file_map_chunk {
	uint64_t *blocks;		/**< Array of blocks in the chunk. */
	bitmap_t bitmap;		/**< Bitmap indicating which blocks are cached. */
} file_map_chunk_t;

/** Size we wish for each chunk to cover. */
#define CHUNK_SIZE	262144

/** Slab cache for file map structures. */
static slab_cache_t *file_map_cache;

/** File map constructor.
 * @param obj		Object to construct.
 * @param data		Unused.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int file_map_ctor(void *obj, void *data, int kmflag) {
	file_map_t *map = obj;

	mutex_init(&map->lock, "file_map_lock", 0);
	avl_tree_init(&map->chunks);
	return 0;
}

/** Create a new file map.
 * @param blksize	Size of one block of the file the map is for.
 * @param ops		Operations structure.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to created file map structure. */
file_map_t *file_map_create(size_t blksize, file_map_ops_t *ops, void *data) {
	file_map_t *map;

	if(blksize > PAGE_SIZE) {
		fatal("Block size too big");
	} else if(!is_pow2(blksize)) {
		fatal("Block size is not a power of 2");
	}

	map = slab_cache_alloc(file_map_cache, MM_SLEEP);
	map->block_size = blksize;
	map->blocks_per_chunk = CHUNK_SIZE / blksize;
	map->ops = ops;
	map->data = data;
	return map;
}

/** Destroy a file map.
 * @param map		Map to destroy. */
void file_map_destroy(file_map_t *map) {
	file_map_chunk_t *chunk;

	AVL_TREE_FOREACH(&map->chunks, iter) {
		chunk = avl_tree_entry(iter, file_map_chunk_t);

		kfree(chunk->blocks);
		bitmap_destroy(&chunk->bitmap);
		kfree(chunk);
	}

	slab_cache_free(file_map_cache, map);
}

/** Look up a block in a file map.
 * @param map		Map to look up in.
 * @param num		Block number to look up.
 * @param rawp		Where to store raw block number.
 * @return		0 on success, negative error code on failure. */
int file_map_lookup(file_map_t *map, uint64_t num, uint64_t *rawp) {
	file_map_chunk_t *chunk;
	size_t chunk_entry;
	key_t chunk_num;
	int ret;

	mutex_lock(&map->lock);

	chunk_num = num / map->blocks_per_chunk;
	chunk_entry = num % map->blocks_per_chunk;

	/* If the chunk is already allocated, see if the block is cached in it,
	 * else allocate a new chunk. */
	if((chunk = avl_tree_lookup(&map->chunks, chunk_num))) {
		if(bitmap_test(&chunk->bitmap, chunk_entry)) {
			*rawp = chunk->blocks[chunk_entry];
			mutex_unlock(&map->lock);
			return 0;
		}
	} else {
		chunk = kmalloc(sizeof(file_map_chunk_t), MM_SLEEP);
		bitmap_init(&chunk->bitmap, map->blocks_per_chunk, NULL, MM_SLEEP);
		chunk->blocks = kmalloc(sizeof(uint64_t) * map->blocks_per_chunk, MM_SLEEP);
		avl_tree_insert(&map->chunks, chunk_num, chunk, NULL);
	}

	/* Look up the block. */
	if((ret = map->ops->lookup(map, num, &chunk->blocks[chunk_entry])) != 0) {
		mutex_unlock(&map->lock);
		return ret;
	}

	bitmap_set(&chunk->bitmap, chunk_entry);
	*rawp = chunk->blocks[chunk_entry];
	mutex_unlock(&map->lock);
	return 0;
}

/** Invalidate entries in a file map.
 * @param map		Map to invalidate in.
 * @param start		Start block to invalidate from.
 * @param count		Number of blocks to invalidate. */
void file_map_invalidate(file_map_t *map, uint64_t start, uint64_t count) {
	file_map_chunk_t *chunk;
	uint64_t i;

	mutex_lock(&map->lock);

	for(i = start; i < (start + count); i++) {
		if(!(chunk = avl_tree_lookup(&map->chunks, i / map->blocks_per_chunk))) {
			continue;
		}

		bitmap_clear(&chunk->bitmap, i % map->blocks_per_chunk);

		/* Free the chunk if it is now empty. */
		if(bitmap_ffs(&chunk->bitmap) < 0) {
			avl_tree_remove(&map->chunks, i / map->blocks_per_chunk);
			kfree(chunk->blocks);
			bitmap_destroy(&chunk->bitmap);
			kfree(chunk);
		}
	}

	mutex_unlock(&map->lock);
}

/** Read a page from a file using a file map.
 *
 * Helper function for a VM cache to read a page from a file using its file
 * map to help read from the source device. If this function is used, the
 * operations structure for the map must have read_block set. The cache's data
 * pointer must be a pointer to the file map.
 *
 * @param cache		Cache being read into.
 * @param buf		Buffer to read into.
 * @param offset	Offset into the file to read from.
 * @param nonblock	Whether the operation is required to not block.
 *
 * @return		0 on success, negative error code on failure.
 */
int file_map_read_page(vm_cache_t *cache, void *buf, offset_t offset, bool nonblock) {
	file_map_t *map = cache->data;
	uint64_t start, raw;
	size_t count, i;
	int ret;

	assert(map);
	assert(map->ops->read_block);

	start = offset / map->block_size;
	count = PAGE_SIZE / map->block_size;

	for(i = 0; i < count; i++, buf += map->block_size) {
		if((ret = file_map_lookup(map, start + i, &raw)) != 0) {
			return ret;
		} else if((ret = map->ops->read_block(map, buf, raw, nonblock)) != 0) {
			return ret;
		}
	}

	return 0;
}

/** Write a page to a file using a file map.
 *
 * Helper function for a VM cache to write a page to a file using its file
 * map to help write to the source device. If this function is used, the
 * operations structure for the map must have write_block set. The cache's data
 * pointer must be a pointer to the file map.
 *
 * @param cache		Cache being written from.
 * @param buf		Buffer containing data to write.
 * @param offset	Offset into the file to write to.
 * @param nonblock	Whether the operation is required to not block.
 *
 * @return		0 on success, negative error code on failure.
 */
int file_map_write_page(vm_cache_t *cache, const void *buf, offset_t offset, bool nonblock) {
	file_map_t *map = cache->data;
	uint64_t start, raw;
	size_t count, i;
	int ret;

	assert(map);
	assert(map->ops->write_block);

	start = offset / map->block_size;
	count = PAGE_SIZE / map->block_size;

	for(i = 0; i < count; i++, buf += map->block_size) {
		if((ret = file_map_lookup(map, start + i, &raw)) != 0) {
			return ret;
		} else if((ret = map->ops->write_block(map, buf, raw, nonblock)) != 0) {
			return ret;
		}
	}

	return 0;
}

/** VM cache operations using a file map to read/write blocks.
 * @note		Cache data pointer should be set to a pointer to the
 *			file map. */
vm_cache_ops_t file_map_vm_cache_ops = {
	.read_page = file_map_read_page,
	.write_page = file_map_write_page,
};

/** Initialise the file map slab cache. */
static void __init_text file_map_init(void) {
	file_map_cache = slab_cache_create("file_map_cache", sizeof(file_map_t),
	                                   0, file_map_ctor, NULL, NULL, NULL,
	                                   SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(file_map_init);
