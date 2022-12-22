/*
 * Copyright (C) 2009-2022 Alex Smith
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

#include <sync/spinlock.h>

struct page;

/** Structure describing a page in memory. */
typedef struct page {
    list_t header;                  /**< Link to page queue. */

    /** Basic page information. */
    phys_ptr_t addr;                /**< Physical address of page. */
    uint8_t range;                  /**< Memory range that the page belongs to. */
    uint8_t state;                  /**< State of the page. */
    atomic_uint16_t flags;          /**< Flags for the page. */

    /** Information about how the page is being used. */
    refcount_t count;               /**< Reference count for use by owner. */
    void *private;                  /**< Private data pointer for the owner. */
} page_t;

/** Possible states of a page. */
enum {
    /**
     * Pages which have been allocated are are currently in-use.
     */
    PAGE_STATE_ALLOCATED,

    /**
     * Pages that are not currently mapped, but are holding cached data. Pages
     * are taken from this queue and freed up when the number of free pages
     * gets low.
     */
    PAGE_STATE_CACHED_CLEAN,

    /**
     * Pages which have been modified and need to be written to their source.
     * There is a thread (the page writer) that periodically takes pages off
     * this queue and writes them. This is used by the cache system to ensure
     * that modifications to data get written to the source soon, rather than
     * staying in memory for a long time without being written.
     */
    PAGE_STATE_CACHED_DIRTY,

    /**
     * Free. This state does not correspond to a page queue, since free pages
     * are managed in separate free lists.
     */
    PAGE_STATE_FREE,

    PAGE_STATE_COUNT,
    PAGE_QUEUE_COUNT = PAGE_STATE_FREE,
};

/** Page flags. */
enum {
    /**
     * Page has been written to. This is set when unmapping a page by arch-
     * specific MMU code if any writes actually occurred to a page while mapped
     * writable.
     */
    PAGE_FLAG_DIRTY = (1<<0),
};

/** Structure containing physical memory usage statistics. */
typedef struct page_stats {
    /** Total available memory. */
    uint64_t total;

    /** Amount of memory per state. */
    uint64_t states[PAGE_STATE_COUNT];
} page_stats_t;

extern bool page_init_done;

/** Atomically adds the given flag(s) to the page's flags.
 * @param page          Page to set for.
 * @param flags         Flag(s) to set. */
static inline void page_set_flag(page_t *page, uint16_t flags) {
    atomic_fetch_or(&page->flags, flags);
}

/** Atomically clears the given flag(s) from the page's flags.
 * @param page          Page to set for.
 * @param flags         Flag(s) to clear. */
static inline void page_clear_flag(page_t *page, uint16_t flags) {
    atomic_fetch_and(&page->flags, ~flags);
}

/** Gets a page's flags.
 * @param page          Page to get from.
 * @return              Page flags. */
static inline uint16_t page_flags(page_t *page) {
    return atomic_load(&page->flags);
}

extern void page_set_state(page_t *page, uint8_t state);
extern page_t *page_lookup(phys_ptr_t addr);
extern page_t *page_alloc(unsigned mmflag);
extern void page_free(page_t *page);
extern page_t *page_copy(page_t *page, unsigned mmflag);

extern void page_stats(page_stats_t *stats);

extern void page_add_memory_range(phys_ptr_t start, phys_ptr_t end, unsigned freelist);

extern phys_ptr_t page_early_alloc(void);

extern void arch_page_init(void);

extern void page_early_init(void);
extern void page_init(void);
extern void page_daemon_init(void);
extern void page_late_init(void);
