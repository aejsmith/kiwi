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
 *
 * This file contains everything related to the management of physical memory.
 * We keep track of physical memory with the page_t structure: each page of
 * usable memory has one of these representing it, that tracks the state of the
 * page and how it is being used.
 *
 * Each usable memory range in the range list provided by the loader is stored
 * in a global array and has an array of page_t's allocated for it. This global
 * array is called the page database, and is used to quickly look up the
 * structure associated with a physical address.
 *
 * There are a number of queues that a page can be placed in depending on its
 * state, see PAGE_STATE_*. The movement of pages between queues as required is
 * mostly left up to the users of the pages: pages will just be placed on the
 * allocated queue when first allocated, and must be moved manually using
 * page_set_state().
 *
 * Free pages are stored in a number of lists. Allocating a single page is just
 * a matter of popping a page from the first list that has free pages. The
 * lists are separated in a arch-specific manner. This is done to improve
 * allocation speed with commonly used minimum/maximum address constraints. For
 * example, AMD64 separates the lists into below 16MB (ISA DMA), below 4GB
 * (devices that use 32-bit DMA addresses) and anything else, since these are
 * the most likely constraints that will be used. Allocations using these
 * constraints can be satisfied simply by popping a page from an appropriate
 * list.
 *
 * The allocation code is not optimised for quick allocations of contiguous
 * ranges of pages, or pages with alignment/boundary constraints, as it is
 * assumed that this will not be done frequently (e.g. in driver initialization
 * routines). The method used to do this is to search through the entire page
 * database to find free pages that satisfy the constraints.
 *
 * Locking rules:
 *  - Free page lock must be held to set a page's state to PAGE_STATE_FREE, or
 *    to change it away from PAGE_STATE_FREE.
 *  - Free page lock and a page queue lock cannot be held at the same time.
 *
 * TODO:
 *  - Pre-zero free pages while idle.
 *  - Reservations of pages for allocations from userspace. When swap is
 *    implemented, the count of memory available to reserve will include swap
 *    space. This means that allocation will not overcommit memory.
 */

#include <device/device.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/page_cache.h>
#include <mm/phys.h>

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

#include "trace.h"

/** Define to enable debug output. */
//#define DEBUG_PAGE

#ifdef DEBUG_PAGE
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Boot memory range structure. */
typedef struct boot_range {
    phys_ptr_t start;               /**< Start of the range. */
    phys_size_t size;               /**< Total size of the range. */
    phys_size_t allocated;          /**< Space allocated in the range. */
    unsigned freelist;              /**< Free page list index. */
} boot_range_t;

/** Structure describing a range of physical memory. */
typedef struct memory_range {
    phys_ptr_t start;               /**< Start of the range. */
    phys_ptr_t end;                 /**< End of the range. */
    page_t *pages;                  /**< Pages in the range. */
    unsigned freelist;              /**< Free page list index. */
} memory_range_t;

/** Structure containing a page queue. */
typedef struct page_queue {
    list_t pages;                   /**< List of pages. */
    page_num_t count;               /**< Number of pages in the queue. */
    spinlock_t lock;                /**< Lock to protect the queue. */
} page_queue_t;

/** Structure containing a free page list. */
typedef struct page_freelist {
    list_t pages;                   /**< Pages in the list. */
    phys_ptr_t min_addr;            /**< Lowest start address contained in the list. */
    phys_ptr_t max_addr;            /**< Highest end address contained in the list. */
} page_freelist_t;

/** Page writer settings. */
#define PAGE_WRITER_INTERVAL        secs_to_nsecs(4)
#define PAGE_WRITER_MAX_PER_RUN     128

/** Maximum number of memory ranges. */
#define MEMORY_RANGE_MAX            32

/** Total usable page count. */
static page_num_t total_page_count;

/** Allocated page queues. */
static page_queue_t page_queues[PAGE_QUEUE_COUNT];

/** Free page list. */
static page_freelist_t free_page_lists[PAGE_FREE_LIST_COUNT];
static MUTEX_DEFINE(free_page_lock, 0);

/** Physical memory ranges. */
static memory_range_t memory_ranges[MEMORY_RANGE_MAX];
static size_t memory_range_count;

/** Free memory range descriptors for early page allocations. */
static boot_range_t boot_ranges[MEMORY_RANGE_MAX] __init_data;
static size_t boot_range_count __init_data = 0;

/** Whether the physical memory manager has been initialized. */
bool page_init_done;

#if CONFIG_PAGE_TRACING

static const char *trace_skip_names[] = {
    "page_copy", "dma_alloc", "dma_free",
};

static __always_inline void *trace_return_address(void) {
    return mm_trace_return_address(trace_skip_names, array_size(trace_skip_names));
}

#endif /* CONFIG_PAGE_TRACING */

static void page_writer(void *arg1, void *arg2) {
    page_queue_t *queue = &page_queues[PAGE_STATE_CACHED_DIRTY];
    LIST_DEFINE(marker);

    while (true) {
        /* TODO: When low on memory, should write pages more often. */
        delay(PAGE_WRITER_INTERVAL);

        /* Place the marker at the beginning of the queue to begin with. */
        spinlock_lock(&queue->lock);
        list_prepend(&queue->pages, &marker);

        /* Write pages until we've reached the maximum number of pages per
         * iteration, or until we reach the end of the queue. */
        size_t written = 0;
        while (written < PAGE_WRITER_MAX_PER_RUN && marker.next != &queue->pages) {
            /* Take the page and move the marker after it. */
            page_t *page = list_entry(marker.next, page_t, header);
            list_add_after(&page->header, &marker);

            /*
             * Try to mark the page as busy. If it was not previously busy, we
             * are able to try to write it. Otherwise, something else is working
             * on this page, so we ignore it for now.
             *
             * Successfully setting this flag guarantees that the page's cache
             * will not be destroyed until we unset it.
             */
            if (page_set_flag(page, PAGE_BUSY) & PAGE_BUSY)
                continue;

            spinlock_unlock(&queue->lock);

            /* Write out the page. This will clear the busy flag. */
            if (page_cache_flush_page(page) == STATUS_SUCCESS) {
                dprintf("page: page writer wrote page 0x%" PRIxPHYS "\n", page->addr);
                written++;
            }

            spinlock_lock(&queue->lock);
        }

        /* Remove the marker and unlock. */
        list_remove(&marker);
        spinlock_unlock(&queue->lock);
    }
}

static inline void page_queue_append(unsigned index, page_t *page) {
    assert(list_empty(&page->header));

    spinlock_lock(&page_queues[index].lock);
    list_append(&page_queues[index].pages, &page->header);
    page_queues[index].count++;
    spinlock_unlock(&page_queues[index].lock);
}

static inline void page_queue_remove(unsigned index, page_t *page) {
    spinlock_lock(&page_queues[index].lock);
    list_remove(&page->header);
    page_queues[index].count--;
    spinlock_unlock(&page_queues[index].lock);
}

static void remove_page_from_current_queue(page_t *page) {
    /* Check that we have a valid current state. */
    if (unlikely(page->state >= PAGE_QUEUE_COUNT))
        fatal("Page 0x%" PRIxPHYS " has invalid state (%u)\n", page->addr, page->state);

    page_queue_remove(page->state, page);
}

/**
 * Sets the state of a page. This cannot be used to move a page from or to
 * PAGE_STATE_FREE, only between allocated states.
 *
 * Users of this function should implement appropriate synchronisation to
 * ensure that concurrent calls to this function should not occur on the same
 * page, since updates to members on the page_t are not synchronised.
 *
 * @param page          Page to set the state of.
 * @param state         New state for the page.
 */
void page_set_state(page_t *page, uint8_t state) {
    /* Remove from current queue. */
    remove_page_from_current_queue(page);

    assert(state < PAGE_QUEUE_COUNT);

    /* Set new state and push on the new queue. */
    page->state = state;
    page_queue_append(state, page);
}

/** Looks up the page structure for a physical address.
 * @param addr          Address to look up.
 * @return              Pointer to page structure if found, null if not. */
page_t *page_lookup(phys_ptr_t addr) {
    assert(!(addr % PAGE_SIZE));

    for (size_t i = 0; i < memory_range_count; i++) {
        if (addr >= memory_ranges[i].start && addr < memory_ranges[i].end)
            return &memory_ranges[i].pages[(addr - memory_ranges[i].start) >> PAGE_WIDTH];
    }

    return NULL;
}

/** Allocates a page.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to structure for allocated page. */
page_t *page_alloc(uint32_t mmflag) {
    assert((mmflag & (MM_WAIT | MM_ATOMIC)) != (MM_WAIT | MM_ATOMIC));

    preempt_disable();
    mutex_lock(&free_page_lock);

    /* Attempt to allocate from each of the lists. */
    for (unsigned i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
        if (list_empty(&free_page_lists[i].pages))
            continue;

        /* Get the page and mark it as allocated. */
        page_t *page = list_first(&free_page_lists[i].pages, page_t, header);
        list_remove(&page->header);

        page->state = PAGE_STATE_ALLOCATED;

        /* No longer require the lock. Must be released before attempting to
         * zero the page as that might require another allocation, which would
         * lead to a nested locking error. */
        mutex_unlock(&free_page_lock);

        /* Put the page onto the allocated queue. */
        page_queue_append(PAGE_STATE_ALLOCATED, page);

        /* If we require a zero page, clear it now. */
        if (mmflag & MM_ZERO) {
            void *mapping = phys_map(page->addr, PAGE_SIZE, mmflag & MM_FLAG_MASK);
            if (unlikely(!mapping)) {
                page_free(page);
                preempt_enable();
                return NULL;
            }

            memset(mapping, 0, PAGE_SIZE);
            phys_unmap(mapping, PAGE_SIZE);
        }

        preempt_enable();

        #if CONFIG_PAGE_TRACING
            kprintf(LOG_DEBUG, "alloc: 0x%" PRIxPHYS " page %pB\n", page->addr, trace_return_address());
        #endif

        dprintf("page: allocated page 0x%" PRIxPHYS " (list: %u)\n", page->addr, i);
        return page;
    }

    // TODO: Reclaim/wait for memory.
    if (mmflag & MM_BOOT) {
        fatal("Unable to satisfy boot page allocation");
    } else if (mmflag & MM_WAIT) {
        /* TODO: Try harder. */
        fatal("TODO: Reclaim/wait for memory");
    }

    mutex_unlock(&free_page_lock);
    preempt_enable();
    return NULL;
}

static void page_free_internal(page_t *page) {
    assert(!refcount_get(&page->count));

    /* Reset the page structure to a clear state. */
    atomic_store(&page->flags, 0);
    page->state       = PAGE_STATE_FREE;
    page->cache_entry = NULL;

    /* Push it onto the appropriate list. */
    list_prepend(&free_page_lists[memory_ranges[page->range].freelist].pages, &page->header);
}

/** Frees a page.
 * @param page          Page to free. */
void page_free(page_t *page) {
    if (unlikely(page->state == PAGE_STATE_FREE))
        fatal("Attempting to free already free page 0x%" PRIxPHYS, page->addr);

    /* Remove from current queue. */
    remove_page_from_current_queue(page);

    mutex_lock(&free_page_lock);
    page_free_internal(page);
    mutex_unlock(&free_page_lock);

    #if CONFIG_PAGE_TRACING
        kprintf(LOG_DEBUG, "free: 0x%" PRIxPHYS " page %pB\n", page->addr, trace_return_address());
    #endif

    dprintf(
        "page: freed page 0x%" PRIxPHYS " (list: %u)\n",
        page->addr, memory_ranges[page->range].freelist);
}

/** Creates a copy of a page.
 * @param page          Page to copy.
 * @param mmflag        Allocation flags.
 * @return              Pointer to new page structure on success, NULL on
 *                      failure. */
page_t *page_copy(page_t *page, uint32_t mmflag) {
    assert(page);

    page_t *dest = page_alloc(mmflag);
    if (unlikely(!dest)) {
        return NULL;
    } else if (unlikely(!phys_copy(dest->addr, page->addr, mmflag))) {
        page_free(dest);
        return NULL;
    }

    return dest;
}

/** Fast path for phys_alloc() (1 page, only minimum/maximum address). */
static page_t *phys_alloc_fastpath(phys_ptr_t min_addr, phys_ptr_t max_addr) {
    /* Maximum of 2 possible partial fits. */
    unsigned partial_fits[2];
    unsigned partial_fit_count = 0;

    /* On the first pass through, we try to allocate from all free lists that
     * are guaranteed to fit these constraints. */
    for (unsigned i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
        page_freelist_t *list = &free_page_lists[i];
        if (!list->min_addr && !list->max_addr)
            continue;

        phys_ptr_t base = max(min_addr, list->min_addr);
        phys_ptr_t end  = min(max_addr - 1, list->max_addr - 1);

        if (base == list->min_addr && end == (list->max_addr - 1)) {
            /* Exact fit. */
            dprintf(
                "page: free list %u can satisfy [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
                i, min_addr, max_addr);
            if (list_empty(&list->pages))
                continue;

            return list_first(&list->pages, page_t, header);
        } else if (end > base) {
            /* Partial fit, record to check in the second pass. */
            partial_fits[partial_fit_count++] = i;
        }
    }

    /* Check any lists that were determined to be a partial fit. */
    for (unsigned i = 0; i < partial_fit_count; i++) {
        dprintf(
            "page: free list %u can partially satisfy [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
            partial_fits[i], min_addr, max_addr);

        /* Check if there are any pages that can fit. */
        page_freelist_t *list = &free_page_lists[partial_fits[i]];
        list_foreach(&list->pages, iter) {
            page_t *page = list_entry(iter, page_t, header);

            if (page->addr >= min_addr && (!max_addr || page->addr < max_addr))
                return page;
        }
    }

    return NULL;
}

/** Slow path for phys_alloc(). */
static page_t *phys_alloc_slowpath(
    page_num_t count, phys_ptr_t align, phys_ptr_t boundary, phys_ptr_t min_addr,
    phys_ptr_t max_addr)
{
    if (boundary != 0)
        fatal("TODO: Implement boundary constraint");

    /* Lovely and slow! Scan each page in each physical range to try to find a
     * set of pages that satisfy the allocation. Because we hold the free pages
     * lock, it is guaranteed that no pages will enter or leave the free state
     * (they can still move between other states) while we are working. */
    for (size_t i = 0; i < memory_range_count; i++) {
        memory_range_t *range = &memory_ranges[i];
        page_num_t total = 0;

        /* Check if this range contains pages in the requested range. */
        phys_ptr_t match_start = max(min_addr, range->start);
        phys_ptr_t match_end   = min(max_addr - 1, range->end - 1);
        if (match_end <= match_start)
            continue;

        /* Scan pages in the range. */
        phys_ptr_t start = (match_start - range->start) / PAGE_SIZE;
        phys_ptr_t end   = ((match_end - range->start) + 1) / PAGE_SIZE;
        page_num_t index;
        for (page_num_t j = start; j < end; j++) {
            if (!total) {
                /* Check if this is a suitable starting page. */
                if (range->pages[j].addr & (align - 1))
                    continue;

                index = j;
            }

            /* Check if the page is free. */
            if (range->pages[j].state != PAGE_STATE_FREE) {
                total = 0;
                continue;
            }

            if (++total == count)
                return &range->pages[index];
        }
    }

    return NULL;
}

/**
 * Allocates a range of contiguous physical memory, with constraints on the
 * location of the allocation. All arguments must be a multiple of the system
 * page size, and any constraints which are not required should be specified
 * as 0. It is intended that this function is not used regularly, for example
 * within driver initialization routines, as it is not optimised for fast
 * allocations. It is, however, optimised for single-page allocations with only
 * certain arch-specific minimum/maximum address constraints, for example below
 * 16MB or below 4GB on AMD64.
 *
 * @param size          Size of the range to allocate.
 * @param align         Required alignment of the range (power of 2).
 * @param boundary      Boundary that the range cannot cross (power of 2).
 * @param min_addr      Minimum start address of the range.
 * @param max_addr      Maximum end address of the range.
 * @param mmflag        Allocation behaviour flags.
 * @param _base         Where to store address of the allocation on success.
 *
 * @return              Status code describing the result of the operation. This
 *                      cannot fail if MM_WAIT is set.
 */
status_t phys_alloc(
    phys_size_t size, phys_ptr_t align, phys_ptr_t boundary,
    phys_ptr_t min_addr, phys_ptr_t max_addr, uint32_t mmflag,
    phys_ptr_t *_base)
{
    if (align == 0)
        align = PAGE_SIZE;

    assert(size);
    assert(!(size % PAGE_SIZE));
    assert(!(align % PAGE_SIZE));
    assert(is_pow2(align));
    assert(!(boundary % PAGE_SIZE));
    assert(!boundary || is_pow2(boundary));
    assert(!(min_addr % PAGE_SIZE));
    assert(!(max_addr % PAGE_SIZE));
    assert(!(min_addr && max_addr) || max_addr > min_addr);
    assert((mmflag & (MM_WAIT | MM_ATOMIC)) != (MM_WAIT | MM_ATOMIC));

    /* Work out how many pages we need to allocate. */
    page_num_t count = size / PAGE_SIZE;

    preempt_disable();
    mutex_lock(&free_page_lock);

    /* Single-page allocations with no constraints or only min_addr/max_addr
     * constraints can be performed quickly. */
    page_t *pages = (count == 1 && align <= PAGE_SIZE && boundary == 0)
        ? phys_alloc_fastpath(min_addr, max_addr)
        : phys_alloc_slowpath(count, align, boundary, min_addr, max_addr);
    if (unlikely(!pages)) {
        if (mmflag & MM_BOOT) {
            fatal("Unable to satisfy boot allocation of %" PRIu32 " page(s)", count);
        } else if (mmflag & MM_WAIT) {
            fatal("TODO: Reclaim/wait for memory");
        }

        mutex_unlock(&free_page_lock);
        preempt_enable();
        return STATUS_NO_MEMORY;
    }

    /* Remove the pages from the free list and mark them as allocated. */
    for (page_num_t i = 0; i < count; i++) {
        list_remove(&pages[i].header);
        pages[i].state = PAGE_STATE_ALLOCATED;
    }

    /* Release the lock (see locking rules). */
    mutex_unlock(&free_page_lock);

    /* Put the pages onto the allocated queue. Pages will have already been
     * marked as allocated. */
    for (page_num_t i = 0; i < count; i++)
        page_queue_append(PAGE_STATE_ALLOCATED, &pages[i]);

    /* If we require the range to be zero, clear it now. */
    if (mmflag & MM_ZERO) {
        void *mapping = phys_map(pages->addr, size, mmflag & MM_FLAG_MASK);
        if (unlikely(!mapping)) {
            phys_free(pages->addr, size);
            preempt_enable();
            return STATUS_NO_MEMORY;
        }

        memset(mapping, 0, size);
        phys_unmap(mapping, size);
    }

    preempt_enable();

    #if CONFIG_PAGE_TRACING
        kprintf(LOG_DEBUG, "alloc: 0x%" PRIxPHYS " phys %pB\n", pages->addr, trace_return_address());
    #endif

    dprintf(
        "page: allocated page range [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
        pages->addr, pages->addr + size);

    *_base = pages->addr;
    return STATUS_SUCCESS;
}

/** Frees a range of physical memory.
 * @param base          Base address of range.
 * @param size          Size of range. */
void phys_free(phys_ptr_t base, phys_size_t size) {
    assert(!(base % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));
    assert(size);
    assert((base + size) > base);

    page_t *pages = page_lookup(base);
    if (unlikely(!pages))
        fatal("Invalid base address 0x%" PRIxPHYS, base);

    /* Ranges allocated by phys_alloc() will not span across a physical range
     * boundary. Check that the caller is not trying to free across one. */
    if (unlikely((base + size) > memory_ranges[pages->range].end))
        fatal("Invalid free across range boundary");

    /* Remove each page in the range from its current queue. */
    for (page_num_t i = 0; i < (size / PAGE_SIZE); i++) {
        if (unlikely(pages[i].state == PAGE_STATE_FREE)) {
            fatal(
                "Page 0x%" PRIxPHYS " in range [0x%" PRIxPHYS ",0x%" PRIxPHYS ") already free",
                pages[i].addr, base, base + size);
        }

        remove_page_from_current_queue(&pages[i]);
    }

    mutex_lock(&free_page_lock);

    /* Free each page. */
    for (page_num_t i = 0; i < size / PAGE_SIZE; i++)
        page_free_internal(&pages[i]);

    mutex_unlock(&free_page_lock);

    #if CONFIG_PAGE_TRACING
        kprintf(LOG_DEBUG, "free: 0x%" PRIxPHYS " phys %pB\n", base, trace_return_address());
    #endif

    dprintf(
        "page: freed page range [0x%" PRIxPHYS ",0x%" PRIxPHYS ") (list: %u)\n",
        base, base + size, memory_ranges[pages->range].freelist);
}

typedef struct device_phys_alloc_resource {
    phys_ptr_t base;
    phys_size_t size;
} device_phys_alloc_resource_t;

static void device_phys_alloc_resource_release(device_t *device, void *data) {
    device_phys_alloc_resource_t *resource = data;

    phys_free(resource->base, resource->size);
}

/**
 * Allocates a range of contiguous physical memory, as a device-managed resource
 * (will be freed when the device is destroyed).
 *
 * @see                 phys_alloc().
 *
 * @param device        Device to register to.
 */
status_t device_phys_alloc(
    device_t *device, phys_size_t size, phys_ptr_t align, phys_ptr_t boundary,
    phys_ptr_t min_addr, phys_ptr_t max_addr, uint32_t mmflag, phys_ptr_t *_base)
{
    phys_ptr_t base;
    status_t ret = phys_alloc(size, align, boundary, min_addr, max_addr, mmflag, &base);
    if (ret == STATUS_SUCCESS) {
        device_phys_alloc_resource_t *resource = device_resource_alloc(
            sizeof(device_phys_alloc_resource_t), device_phys_alloc_resource_release, MM_KERNEL);

        resource->base = base;
        resource->size = size;

        device_resource_register(device, resource);

        *_base = base;
    }

    return ret;
}

/** Gets physical memory usage statistics.
 * @param stats         Structure to fill in. */
void page_stats(page_stats_t *stats) {
    stats->total                   = total_page_count * PAGE_SIZE;
    stats->states[PAGE_STATE_FREE] = stats->total;

    for (size_t i = 0; i < PAGE_QUEUE_COUNT; i++) {
        stats->states[i]                = page_queues[i].count * PAGE_SIZE;
        stats->states[PAGE_STATE_FREE] -= stats->states[i];
    }
}

/** Print details about physical memory usage. */
static kdb_status_t kdb_cmd_page(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<addr>]\n\n", argv[0]);

        kdb_printf("Prints out a list of all usable page ranges and information about physical\n");
        kdb_printf("memory usage, or details of a single page.\n");
        return KDB_SUCCESS;
    } else if (argc != 1 && argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    if (argc == 2) {
        uint64_t addr;
        if (kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS) {
            return KDB_FAILURE;
        } else if (addr % PAGE_SIZE) {
            kdb_printf("Address must be page aligned.\n");
            return KDB_FAILURE;
        }

        page_t *page = page_lookup((phys_ptr_t)addr);
        if (!page) {
            kdb_printf("404 Page Not Found\n");
            return KDB_FAILURE;
        }

        kdb_printf("Page 0x%" PRIxPHYS " (%p) (Range: %u)\n", page->addr, page, page->range);
        kdb_printf("=================================================\n");
        kdb_printf("state:       %d\n", page->state);
        kdb_printf("flags:       0x%x\n", page_flags(page));
        kdb_printf("count:       %d\n", page->count);
        kdb_printf("cache_entry: %p\n", page->cache_entry);
    } else {
        kdb_printf("Start              End                Freelist Pages\n");
        kdb_printf("=====              ===                ======== =====\n");

        for (size_t i = 0; i < memory_range_count; i++) {
            kdb_printf(
                "0x%-16" PRIxPHYS " 0x%-16" PRIxPHYS " %-8u %p\n",
                memory_ranges[i].start, memory_ranges[i].end,
                memory_ranges[i].freelist, memory_ranges[i].pages);
        }

        page_stats_t stats;
        page_stats(&stats);

        kdb_printf("\n");
        kdb_printf("Statistics\n");
        kdb_printf("==========\n");
        kdb_printf("Total:          %" PRIu64 " KiB\n", stats.total / 1024);
        kdb_printf("Allocated:      %" PRIu64 " KiB\n", stats.states[PAGE_STATE_ALLOCATED] / 1024);
        kdb_printf("Cached (clean): %" PRIu64 " KiB\n", stats.states[PAGE_STATE_CACHED_CLEAN] / 1024);
        kdb_printf("Cached (dirty): %" PRIu64 " KiB\n", stats.states[PAGE_STATE_CACHED_DIRTY] / 1024);
        kdb_printf("Free:           %" PRIu64 " KiB\n", stats.states[PAGE_STATE_FREE] / 1024);
    }

    return KDB_SUCCESS;
}

/**
 * Adds a new range of physical memory. Ranges must be added in lowest to
 * highest order.
 *
 * @param start         Start of range.
 * @param end           End of range.
 * @param freelist      Free page list index for pages in the range.
 */
__init_text void page_add_memory_range(phys_ptr_t start, phys_ptr_t end, unsigned freelist) {
    page_freelist_t *list = &free_page_lists[freelist];

    /* Increase the total page count. */
    total_page_count += (end - start) / PAGE_SIZE;

    /* Update the freelist to include this range. */
    if (!list->min_addr && !list->max_addr) {
        list->min_addr = start;
        list->max_addr = end;
    } else {
        if (start < list->min_addr)
            list->min_addr = start;
        if (end > list->max_addr)
            list->max_addr = end;
    }

    /* If we're contiguous with the previously recorded range (if any) and have
     * the same free list index, just append to it, else add a new range. */
    if (memory_range_count) {
        memory_range_t *prev = &memory_ranges[memory_range_count - 1];
        if (start == prev->end && freelist == prev->freelist) {
            prev->end = end;
            return;
        }
    }

    if (memory_range_count >= MEMORY_RANGE_MAX)
        fatal("Too many physical memory ranges");

    memory_range_t *range = &memory_ranges[memory_range_count++];

    range->start    = start;
    range->end      = end;
    range->freelist = freelist;
}

/** Perform an early page allocation.
 * @return              Physical address of page allocated. */
phys_ptr_t page_early_alloc(void) {
    /* Search for a range with free pages. */
    for (size_t i = 0; i < boot_range_count; i++) {
        if (boot_ranges[i].allocated < boot_ranges[i].size) {
            phys_ptr_t ret = boot_ranges[i].start + boot_ranges[i].allocated;

            boot_ranges[i].allocated += PAGE_SIZE;

            dprintf("page: allocated early page 0x%" PRIxPHYS "\n", ret);
            return ret;
        }
    }

    fatal("Exhausted available memory during boot");
}

static __init_text int boot_range_compare(const void *a, const void *b) {
    const boot_range_t *first  = (const boot_range_t *)a;
    const boot_range_t *second = (const boot_range_t *)b;

    if (first->freelist == second->freelist) {
        return (first->start > second->start) ? -1 : (first->start < second->start);
    } else {
        return (first->freelist < second->freelist) ? -1 : 1;
    }
}

/** Perform early physical memory manager initialization. */
__init_text void page_early_init(void) {
    /* Initialize page queues and freelists. */
    for (unsigned i = 0; i < PAGE_QUEUE_COUNT; i++) {
        list_init(&page_queues[i].pages);
        page_queues[i].count = 0;
        spinlock_init(&page_queues[i].lock, "page_queue_lock");
    }
    for (unsigned i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
        list_init(&free_page_lists[i].pages);
        free_page_lists[i].min_addr = 0;
        free_page_lists[i].max_addr = 0;
    }

    /* First step is to call into arch-specific code to parse the memory map
     * provided by the loader and separate it further as required (i.e. into
     * different free lists). */
    arch_page_init();

    /* And here we have the fun of early memory management bringup. We want to
     * map pages during our initialization, but to do that the MMU code needs to
     * allocate pages. Therefore, we have an early page allocation system for
     * use during initialization that just grabs the first free page it can find
     * from the memory ranges given by the boot loader. The real initialization
     * code will then mark all pages allocated through that function as
     * allocated. */
    kboot_tag_foreach(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
        if (boot_range_count == MEMORY_RANGE_MAX)
            fatal("Memory map contains too many ranges");

        boot_ranges[boot_range_count].start = range->start;
        boot_ranges[boot_range_count].size = range->size;
        boot_ranges[boot_range_count].allocated =
            (range->type == KBOOT_MEMORY_FREE) ? 0 : range->size;

        /* Match up the range against a freelist. Not entirely correct for
         * ranges that straddle across multiples lists, so we just always select
         * the lowest priority (highest index) list. */
        for (unsigned i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
            page_freelist_t *list = &free_page_lists[i];
            if (!list->min_addr && !list->max_addr)
                continue;

            if (range->start <= (list->max_addr - 1) &&
                list->min_addr <= (range->start + range->size - 1))
            {
                boot_ranges[boot_range_count].freelist = i;
            }
        }

        boot_range_count++;
    }

    /* Sort the array of boot ranges by lowest to highest freelist index so
     * that early allocations honor free list priorities. */
    qsort(boot_ranges, boot_range_count, sizeof(boot_range_t), boot_range_compare);
}

/** Initialize the physical memory manager. */
__init_text void page_init(void) {
    kprintf(LOG_NOTICE, "page: usable physical memory ranges:\n");
    for (size_t i = 0; i < memory_range_count; i++) {
        kprintf(
            LOG_NOTICE, " 0x%016" PRIxPHYS " - 0x%016" PRIxPHYS " (%u)\n",
            memory_ranges[i].start, memory_ranges[i].end,
            memory_ranges[i].freelist);
    }

    kprintf(LOG_NOTICE, "page: free list coverage:\n");
    for (size_t i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
        kprintf(
            LOG_NOTICE, " %zu: 0x%016" PRIxPHYS " - 0x%016" PRIxPHYS "\n",
            i, free_page_lists[i].min_addr, free_page_lists[i].max_addr);
    }

    /* Determine how much space we need for the page database. */
    size_t pages_size = round_up(sizeof(page_t) * total_page_count, PAGE_SIZE);
    kprintf(
        LOG_NOTICE, "page: have %" PRIu32 " pages, using %" PRIuPHYS "KiB for page database\n",
        total_page_count, pages_size / 1024);
    if (pages_size > KERNEL_PDB_SIZE)
        fatal("Available RAM exceeds maximum page database size");

    mmu_context_lock(&kernel_mmu_context);

    /* Allocate and map the database. */
    for (size_t i = 0; i < pages_size; i += PAGE_SIZE) {
        mmu_context_map(
            &kernel_mmu_context, KERNEL_PDB_BASE + i, page_early_alloc(),
            MMU_ACCESS_RW, MM_BOOT);
    }

    mmu_context_unlock(&kernel_mmu_context);

    ptr_t addr = KERNEL_PDB_BASE;

    /* Now for each memory range we have, create page structures. */
    for (size_t i = 0; i < memory_range_count; i++) {
        page_num_t count = (memory_ranges[i].end - memory_ranges[i].start) / PAGE_SIZE;
        size_t size      = sizeof(page_t) * count;

        memory_ranges[i].pages = (page_t *)addr;
        addr += size;

        /* Initialize each of the pages. */
        memset(memory_ranges[i].pages, 0, size);
        for (page_num_t j = 0; j < count; j++) {
            page_t *page = &memory_ranges[i].pages[j];
            list_init(&page->header);
            page->addr = memory_ranges[i].start + ((phys_ptr_t)j * PAGE_SIZE);
            page->range = i;
        }
    }

    /* Finally, set the state of each page based on the boot allocation
     * information. */
    for (size_t i = 0; i < boot_range_count; i++) {
        for (size_t j = 0; j < boot_ranges[i].size; j += PAGE_SIZE) {
            page_t *page = page_lookup(boot_ranges[i].start + j);
            assert(page);

            if (j >= boot_ranges[i].allocated) {
                page->state = PAGE_STATE_FREE;

                unsigned index = memory_ranges[page->range].freelist;
                list_append(&free_page_lists[index].pages, &page->header);
            } else {
                page->state = PAGE_STATE_ALLOCATED;
                page_queue_append(PAGE_STATE_ALLOCATED, page);
            }
        }
    }

    kdb_register_command(
        "page",
        "Display physical memory usage information.",
        kdb_cmd_page);

    page_init_done = true;
}

/** Initialize the page daemons. */
__init_text void page_daemon_init(void) {
    status_t ret = thread_create("page_writer", NULL, 0, page_writer, NULL, NULL, NULL);
    if (ret != STATUS_SUCCESS)
        fatal("Could not start page writer (%d)", ret);
}

/** Reclaim memory no longer in use after kernel initialization. */
__init_text void page_late_init(void) {
    /* Calculate the location and size of the initialization section. */
    kboot_tag_core_t *core = kboot_tag_iterate(KBOOT_TAG_CORE, NULL);
    phys_ptr_t init_start  = ((ptr_t)__init_seg_start - KERNEL_VIRT_BASE) + core->kernel_phys;
    phys_ptr_t init_end    = ((ptr_t)__init_seg_end - KERNEL_VIRT_BASE) + core->kernel_phys;

    /* It's OK for us to reclaim despite the fact that the KBoot data is
     * contained in memory that will be reclaimed, as nothing should make any
     * allocations or write to reclaimed memory while this is happening. */
    size_t reclaimed = 0;
    kboot_tag_foreach(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
        if (range->type != KBOOT_MEMORY_FREE && range->type != KBOOT_MEMORY_ALLOCATED) {
            /* Must individually free each page in the range, as the KBoot range
             * could be split across more than one of our internal ranges, and
             * frees across range boundaries are not allowed. */
            for (phys_ptr_t addr = range->start; addr < range->start + range->size; addr += PAGE_SIZE)
                phys_free(addr, PAGE_SIZE);

            reclaimed += range->size;
        }
    }

    /* Free the initialization data. Same as above applies. */
    for (phys_ptr_t addr = init_start; addr < init_end; addr += PAGE_SIZE)
        phys_free(addr, PAGE_SIZE);

    reclaimed += init_end - init_start;

    kprintf(LOG_NOTICE, "page: reclaimed %zu KiB of unneeded memory\n", reclaimed / 1024);
}
