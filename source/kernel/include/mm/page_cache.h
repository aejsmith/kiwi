/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Page-based data cache.
 */

#pragma once

#include <lib/avl_tree.h>

#include <mm/page.h>
#include <mm/vm.h>

#include <sync/mutex.h>
#include <sync/spinlock.h>

struct io_request;
struct page_cache;

/** Structure containing operations for a page cache. */
typedef struct page_cache_ops {
    /** Read a page of data from the source.
     * @param cache         Cache being read from.
     * @param buf           Buffer to read into.
     * @param offset        Offset to read from.
     * @return              Status code describing result of operation. */
    status_t (*read_page)(struct page_cache *cache, void *buf, offset_t offset);

    /** Write a page of data to the source.
     * @param cache         Cache to write to.
     * @param buf           Buffer containing data to write.
     * @param offset        Offset to write from.
     * @return              Status code describing result of operation. */
    status_t (*write_page)(struct page_cache *cache, const void *buf, offset_t offset);
} page_cache_ops_t;

/** Structure containing a page-based data cache. */
typedef struct page_cache {
    mutex_t lock;                   /**< Lock protecting cache. */

    avl_tree_t pages;               /**< Tree of pages. */
    offset_t size;                  /**< Size of the cache. */

    list_t waiters;                 /**< Busy page waiters. */
    spinlock_t waiters_lock;        /**< Waiters list lock. */

    const page_cache_ops_t *ops;    /**< Pointer to operations structure. */
    void *private;                  /**< Cache data pointer. */
} page_cache_t;

extern const vm_region_ops_t page_cache_region_ops;

extern status_t page_cache_flush_page(page_t *page);

extern status_t page_cache_io(page_cache_t *cache, struct io_request *request);
extern status_t page_cache_read(page_cache_t *cache, void *buf, size_t size, offset_t offset, size_t *_bytes);
extern status_t page_cache_write(page_cache_t *cache, const void *buf, size_t size, offset_t offset, size_t *_bytes);

extern void page_cache_resize(page_cache_t *cache, offset_t size);
extern status_t page_cache_flush(page_cache_t *cache);

extern page_cache_t *page_cache_create(offset_t size, const page_cache_ops_t *ops, void *private);
extern status_t page_cache_destroy(page_cache_t *cache);

extern void page_cache_init(void);
