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
 */

#ifndef __IO_FILE_MAP_H
#define __IO_FILE_MAP_H

#include <lib/avl.h>
#include <lib/bitmap.h>

#include <sync/mutex.h>

struct file_map;
struct vm_cache_ops;
struct vm_cache;

/** Structure containing operations for a file map.
 * @note		The read/write block functions need only be provided
 *			if the file map VM cache functions are used. */
typedef struct file_map_ops {
	/** Look up a block number.
	 * @param map		Map being looked up in.
	 * @param num		File block number to look up.
	 * @param rawp		Where to store the raw block number.
	 * @return		0 on success, negative error code on failure. */
	int (*lookup)(struct file_map *map, uint64_t num, uint64_t *rawp);

	/** Read a block from the source device.
	 * @param map		Map the read is for.
	 * @param buf		Buffer to read into.
	 * @param num		Raw block number.
	 * @param nonblock	Whether the operation is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*read_block)(struct file_map *map, void *buf, uint64_t num, bool nonblock);

	/** Write a block to the source device.
	 * @param map		Map the write is for.
	 * @param buf		Buffer containing data to write.
	 * @param num		Raw block number.
	 * @param nonblock	Whether the operation is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*write_block)(struct file_map *map, const void *buf, uint64_t num, bool nonblock);
} file_map_ops_t;

/** Structure containing a file map. */
typedef struct file_map {
	mutex_t lock;			/**< Lock to protect map. */
	size_t block_size;		/**< Size of one block. */
	size_t blocks_per_chunk;	/**< Number of blocks per chunk. */
	avl_tree_t chunks;		/**< Tree of chunks. */
	file_map_ops_t *ops;		/**< Operations for the map. */
	void *data;			/**< Implementation-specific data pointer. */
} file_map_t;

extern struct vm_cache_ops file_map_vm_cache_ops;

extern file_map_t *file_map_create(size_t blksize, file_map_ops_t *ops, void *data);
extern void file_map_destroy(file_map_t *map);

extern int file_map_lookup(file_map_t *map, uint64_t num, uint64_t *rawp);
extern void file_map_invalidate(file_map_t *map, uint64_t start, uint64_t count);

extern int file_map_read_page(struct vm_cache *cache, void *buf, offset_t offset, bool nonblock);
extern int file_map_write_page(struct vm_cache *cache, const void *buf, offset_t offset, bool nonblock);

#endif /* __IO_FILE_MAP_H */
