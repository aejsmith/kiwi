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
 */

#ifndef __IO_MAP_H
#define __IO_MAP_H

#include <lib/avl.h>
#include <lib/bitmap.h>

#include <sync/mutex.h>

struct block_map;

/** Structure containing operations for a block map. */
typedef struct block_map_ops {
	/** Look up a block number.
	 * @param map		Map being looked up in.
	 * @param num		Block number to look up.
	 * @param outp		Where to store the data to cache.
	 * @return		0 on success, negative error code on failure. */
	int (*lookup)(struct block_map *map, uint64_t num, uint64_t *outp);
} block_map_ops_t;

/** Structure containing a single range in a block map. */
typedef struct block_map_chunk {
	uint64_t *blocks;		/**< Array of blocks in the chunk. */
	bitmap_t bitmap;		/**< Bitmap indicating which blocks are cached. */
} block_map_chunk_t;

/** Structure containing a block map. */
typedef struct block_map {
	mutex_t lock;			/**< Lock to protect map. */
	size_t blocks_per_chunk;	/**< Number of blocks per chunk. */
	avl_tree_t chunks;		/**< Tree of chunks. */
	block_map_ops_t *ops;		/**< Operations for the map. */
	void *data;			/**< Implementation-specific data pointer. */
} block_map_t;

extern block_map_t *block_map_create(size_t blksize, block_map_ops_t *ops, void *data);
extern void block_map_destroy(block_map_t *map);

extern int block_map_lookup(block_map_t *map, uint64_t num, uint64_t *outp);
extern void block_map_invalidate(block_map_t *map, uint64_t start, uint64_t count);

#endif /* __IO_MAP_H */
