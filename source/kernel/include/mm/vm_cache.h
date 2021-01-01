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
 * @brief               Page-based data cache.
 */

#pragma once

#include <lib/avl_tree.h>

#include <mm/page.h>
#include <mm/vm.h>

#include <sync/mutex.h>

struct io_request;
struct vm_cache;

/** Structure containing operations for a page cache. */
typedef struct vm_cache_ops {
    /** Read a page of data from the source.
     * @note                If not provided, pages that need to be allocated
     *                      will be zero-filled.
     * @param cache         Cache being read from.
     * @param buf           Buffer to read into.
     * @param offset        Offset to read from.
     * @return              Status code describing result of operation. */
    status_t (*read_page)(struct vm_cache *cache, void *buf, offset_t offset);

    /** Write a page of data to the source.
     * @note                If not provided, pages in the cache will never be
     *                      marked as modified.
     * @param cache         Cache to write to.
     * @param buf           Buffer containing data to write.
     * @param offset        Offset to write from.
     * @return              Status code describing result of operation. */
    status_t (*write_page)(struct vm_cache *cache, const void *buf, offset_t offset);

    /** Determine whether a page can be evicted.
     * @note                If not provided, then behaviour will be as though
     *                      the function returns true.
     * @param cache         Cache the page belongs to.
     * @param page          Page to check.
     * @return              Whether the page can be evicted. */
    bool (*evict_page)(struct vm_cache *cache, page_t *page);
} vm_cache_ops_t;

/** Structure containing a page-based data cache. */
typedef struct vm_cache {
    mutex_t lock;                   /**< Lock protecting cache. */
    avl_tree_t pages;               /**< Tree of pages. */
    offset_t size;                  /**< Size of the cache. */
    vm_cache_ops_t *ops;            /**< Pointer to operations structure. */
    void *data;                     /**< Cache data pointer. */
    bool deleted;                   /**< Whether the cache is destroyed. */
} vm_cache_t;

extern vm_region_ops_t vm_cache_region_ops;

extern status_t vm_cache_io(vm_cache_t *cache, struct io_request *request);
extern void vm_cache_resize(vm_cache_t *cache, offset_t size);
extern status_t vm_cache_flush(vm_cache_t *cache);

extern vm_cache_t *vm_cache_create(offset_t size, vm_cache_ops_t *ops, void *data);
extern status_t vm_cache_destroy(vm_cache_t *cache, bool discard);

extern void vm_cache_init(void);
