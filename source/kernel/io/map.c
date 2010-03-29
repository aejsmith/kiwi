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
 * @brief		Block map functions.
 *
 * The functions in this file implement a block map which maps block numbers
 * to whatever the user of the map chooses (e.g. disk location). This is useful
 * in filesystem modules to map block numbers within files to a location on
 * the source device.
 */

#include <io/map.h>

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <fatal.h>
#include <init.h>

/** Size we wish for each chunk to cover. */
#define CHUNK_SIZE	262144

/** Slab cache for block map structures. */
static slab_cache_t *block_map_cache;

/** Block map cache constructor.
 * @param obj		Object to construct.
 * @param data		Unused.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int block_map_ctor(void *obj, void *data, int kmflag) {
	block_map_t *map = obj;

	mutex_init(&map->lock, "block_map_lock", 0);
	avl_tree_init(&map->chunks);
	return 0;
}

/** Create a new block map.
 * @param blksize	Size of one block on the source. This is merely use
 *			to determine how many blocks should be in one chunk.
 * @param ops		Operations structure.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to created block map structure. */
block_map_t *block_map_create(size_t blksize, block_map_ops_t *ops, void *data) {
	block_map_t *map;

	if(blksize > CHUNK_SIZE) {
		fatal("Block size too big");
	} else if(!is_pow2(blksize)) {
		fatal("Block size is not a power of 2");
	}

	map = slab_cache_alloc(block_map_cache, MM_SLEEP);
	map->blocks_per_chunk = CHUNK_SIZE / blksize;
	map->ops = ops;
	map->data = data;
	return map;
}

/** Destroy a block map.
 * @param map		Map to destroy. */
void block_map_destroy(block_map_t *map) {
	block_map_chunk_t *chunk;

	AVL_TREE_FOREACH(&map->chunks, iter) {
		chunk = avl_tree_entry(iter, block_map_chunk_t);

		kfree(chunk->blocks);
		bitmap_destroy(&chunk->bitmap);
		kfree(chunk);
	}

	slab_cache_free(block_map_cache, map);
}

/** Look up a block in a block map.
 * @param map		Map to look up in.
 * @param num		Block number to look up.
 * @param outp		Where to store value looked up.
 * @return		0 on success, negative error code on failure. */
int block_map_lookup(block_map_t *map, uint64_t num, uint64_t *outp) {
	block_map_chunk_t *chunk;
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
			*outp = chunk->blocks[chunk_entry];
			mutex_unlock(&map->lock);
			return 0;
		}
	} else {
		chunk = kmalloc(sizeof(block_map_chunk_t), MM_SLEEP);
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
	*outp = chunk->blocks[chunk_entry];
	mutex_unlock(&map->lock);
	return 0;
}

/** Invalidate entries in a block cache.
 * @param map		Map to invalidate in.
 * @param start		Start block to invalidate from.
 * @param count		Number of entries to invalidate. */
void block_map_invalidate(block_map_t *map, uint64_t start, uint64_t count) {
	block_map_chunk_t *chunk;
	uint64_t i;

	mutex_lock(&map->lock);

	for(i = start; i < count; i++) {
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

/** Initialise the block map slab cache. */
static void __init_text block_map_init(void) {
	block_map_cache = slab_cache_create("block_map_cache", sizeof(block_map_t),
	                                    0, block_map_ctor, NULL, NULL, NULL,
	                                    SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(block_map_init);
