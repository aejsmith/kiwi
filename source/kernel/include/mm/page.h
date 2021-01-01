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
 * @brief               Physical memory management.
 */

#pragma once

#include <arch/page.h>

#include <lib/avl_tree.h>
#include <lib/list.h>
#include <lib/refcount.h>

#include <mm/mm.h>

#include <platform/page.h>

#include <sync/spinlock.h>

struct page;

/** Interface from the memory manager to a page's owner. */
typedef struct page_ops {
    /** Write back a dirty page.
     * @param page          Page to write back.
     * @return              Status code describing result of the operation. */
    status_t (*flush_page)(struct page *page);

    /** Release a page.
     * @param page          Page to release.
     * @param phys          Physical address of page that was unmapped. */
    void (*release_page)(struct page *page);
} page_ops_t;

/** Structure describing a page in memory. */
typedef struct page {
    list_t header;                  /**< Link to page queue. */

    /** Basic page information. */
    phys_ptr_t addr;                /**< Physical address of page. */
    unsigned range;                 /**< Memory range that the page belongs to. */
    unsigned state;                 /**< State of the page. */
    bool modified : 1;              /**< Whether the page has been modified. */
    uint8_t unused: 7;

    /** Information about how the page is being used. */
    page_ops_t *ops;                /**< Operations for the page. */
    void *private;                  /**< Private data pointer for the owner. */
    offset_t offset;                /**< Offset into the owner of the page. */
    refcount_t count;               /**< Reference count for use by owner. */
    avl_tree_node_t avl_link;       /**< Link to AVL tree for use by owner. */
} page_t;

/** Possible states of a page. */
#define PAGE_STATE_ALLOCATED    0   /**< Allocated. */
#define PAGE_STATE_MODIFIED     1   /**< Modified. */
#define PAGE_STATE_CACHED       2   /**< Cached. */
#define PAGE_STATE_FREE         3   /**< Free. */

/** Structure containing physical memory usage statistics. */
typedef struct page_stats {
    uint64_t total;                 /**< Total available memory. */
    uint64_t allocated;             /**< Amount of memory in-use. */
    uint64_t modified;              /**< Amount of memory containing modified data. */
    uint64_t cached;                /**< Amount of memory being used by caches. */
    uint64_t free;                  /**< Amount of free memory. */
} page_stats_t;

extern bool page_init_done;

extern void page_set_state(page_t *page, unsigned state);
extern page_t *page_lookup(phys_ptr_t addr);
extern page_t *page_alloc(unsigned mmflag);
extern void page_free(page_t *page);
extern page_t *page_copy(page_t *page, unsigned mmflag);

extern void page_stats_get(page_stats_t *stats);

extern void page_add_memory_range(phys_ptr_t start, phys_ptr_t end, unsigned freelist);

extern phys_ptr_t page_early_alloc(void);

extern void platform_page_init(void);

extern void page_early_init(void);
extern void page_init(void);
extern void page_daemon_init(void);
extern void page_late_init(void);
