/*
 * Copyright (C) 2009-2021 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               File map.
 *
 * A file map implements a cache for file block number to raw (i.e. on-disk)
 * block number translations. Also provided are VM cache helper functions that
 * can use a file map to handle reading and writing of data pages.
 */

#pragma once

#include <lib/avl_tree.h>

#include <sync/mutex.h>

struct file_map;
struct vm_cache_ops;
struct vm_cache;

/** Structure containing operations for a file map. */
typedef struct file_map_ops {
    /** Look up a block number.
     * @param map           Map being looked up in.
     * @param num           File block number to look up.
     * @param _raw          Where to store the raw block number.
     * @return              Status code describing result of the operation. */
    status_t (*lookup)(struct file_map *map, uint64_t num, uint64_t *_raw);

    /**
     * Block I/O functions. These are only required if the file map VM cache
     * functions are used.
     */

    /** Read a block from the source device.
     * @param map           Map the read is for.
     * @param buf           Buffer to read into.
     * @param num           Raw block number.
     * @param nonblock      Whether the operation is required to not block.
     * @return              Status code describing result of the operation. */
    status_t (*read_block)(struct file_map *map, void *buf, uint64_t num);

    /** Write a block to the source device.
     * @param map           Map the write is for.
     * @param buf           Buffer containing data to write.
     * @param num           Raw block number.
     * @param nonblock      Whether the operation is required to not block.
     * @return              Status code describing result of the operation. */
    status_t (*write_block)(struct file_map *map, const void *buf, uint64_t num);
} file_map_ops_t;

/** Structure containing a file map. */
typedef struct file_map {
    mutex_t lock;                   /**< Lock to protect map. */
    size_t block_size;              /**< Size of one block. */
    size_t blocks_per_chunk;        /**< Number of blocks per chunk. */
    avl_tree_t chunks;              /**< Tree of chunks. */
    const file_map_ops_t *ops;      /**< Operations for the map. */
    void *private;                  /**< Implementation-specific private data. */
} file_map_t;

extern const struct vm_cache_ops file_map_vm_cache_ops;

extern status_t file_map_lookup(file_map_t *map, uint64_t num, uint64_t *_raw);
extern void file_map_invalidate(file_map_t *map, uint64_t start, uint64_t count);

extern status_t file_map_read_page(struct vm_cache *cache, void *buf, offset_t offset);
extern status_t file_map_write_page(struct vm_cache *cache, const void *buf, offset_t offset);

extern file_map_t *file_map_create(size_t block_size, const file_map_ops_t *ops, void *private);
extern void file_map_destroy(file_map_t *map);
