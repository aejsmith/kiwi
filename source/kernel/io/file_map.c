/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		File map functions.
 *
 * The functions in this file implement a cache for file block numbers to raw
 * (i.e. on-disk) block numbers. Also provided are VM cache helper functions
 * that can use data in a file map to handle reading and writing of data pages.
 */

#include <io/file_map.h>

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <assert.h>
#include <status.h>

/** Structure containing a single range in a file map. */
typedef struct file_map_chunk {
	uint64_t *blocks;		/**< Array of blocks in the chunk. */
	bitmap_t bitmap;		/**< Bitmap indicating which blocks are cached. */
	avl_tree_node_t link;		/**< Link to the map. */
} file_map_chunk_t;

/** Size we wish for each chunk to cover. */
#define CHUNK_SIZE	262144

/** Slab cache for file map structures. */
static slab_cache_t *file_map_cache;

/** File map constructor.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void file_map_ctor(void *obj, void *data) {
	file_map_t *map = obj;

	mutex_init(&map->lock, "file_map_lock", 0);
	avl_tree_init(&map->chunks);
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
	} else if(!IS_POW2(blksize)) {
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

	AVL_TREE_FOREACH_SAFE(&map->chunks, iter) {
		chunk = avl_tree_entry(iter, file_map_chunk_t);

		avl_tree_remove(&map->chunks, &chunk->link);
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
 * @return		Status code describing result of the operation. */
status_t file_map_lookup(file_map_t *map, uint64_t num, uint64_t *rawp) {
	file_map_chunk_t *chunk;
	size_t chunk_entry;
	key_t chunk_num;
	status_t ret;

	mutex_lock(&map->lock);

	chunk_num = num / map->blocks_per_chunk;
	chunk_entry = num % map->blocks_per_chunk;

	/* If the chunk is already allocated, see if the block is cached in it,
	 * else allocate a new chunk. */
	chunk = avl_tree_lookup(&map->chunks, chunk_num);
	if(chunk) {
		if(bitmap_test(&chunk->bitmap, chunk_entry)) {
			*rawp = chunk->blocks[chunk_entry];
			mutex_unlock(&map->lock);
			return STATUS_SUCCESS;
		}
	} else {
		chunk = kmalloc(sizeof(file_map_chunk_t), MM_SLEEP);
		bitmap_init(&chunk->bitmap, map->blocks_per_chunk, NULL, MM_SLEEP);
		chunk->blocks = kmalloc(sizeof(uint64_t) * map->blocks_per_chunk, MM_SLEEP);
		avl_tree_insert(&map->chunks, &chunk->link, chunk_num, chunk);
	}

	/* Look up the block. */
	ret = map->ops->lookup(map, num, &chunk->blocks[chunk_entry]);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&map->lock);
		return ret;
	}

	bitmap_set(&chunk->bitmap, chunk_entry);
	*rawp = chunk->blocks[chunk_entry];
	mutex_unlock(&map->lock);
	return STATUS_SUCCESS;
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
		chunk = avl_tree_lookup(&map->chunks, i / map->blocks_per_chunk);
		if(!chunk) {
			continue;
		}

		bitmap_clear(&chunk->bitmap, i % map->blocks_per_chunk);

		/* Free the chunk if it is now empty. */
		if(bitmap_ffs(&chunk->bitmap) < 0) {
			avl_tree_remove(&map->chunks, &chunk->link);
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
 * @return		Status code describing result of the operation.
 */
status_t file_map_read_page(vm_cache_t *cache, void *buf, offset_t offset, bool nonblock) {
	file_map_t *map = cache->data;
	uint64_t start, raw;
	size_t count, i;
	status_t ret;

	assert(map);
	assert(map->ops->read_block);

	start = offset / map->block_size;
	count = PAGE_SIZE / map->block_size;

	for(i = 0; i < count; i++, buf += map->block_size) {
		ret = file_map_lookup(map, start + i, &raw);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		ret = map->ops->read_block(map, buf, raw, nonblock);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation.
 */
status_t file_map_write_page(vm_cache_t *cache, const void *buf, offset_t offset, bool nonblock) {
	file_map_t *map = cache->data;
	uint64_t start, raw;
	size_t count, i;
	status_t ret;

	assert(map);
	assert(map->ops->write_block);

	start = offset / map->block_size;
	count = PAGE_SIZE / map->block_size;

	for(i = 0; i < count; i++, buf += map->block_size) {
		ret = file_map_lookup(map, start + i, &raw);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		ret = map->ops->write_block(map, buf, raw, nonblock);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	return STATUS_SUCCESS;
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
	                                   0, file_map_ctor, NULL, NULL, 0,
	                                   MM_FATAL);
}
INITCALL(file_map_init);
