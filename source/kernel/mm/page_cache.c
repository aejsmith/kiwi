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
 * @brief               Page-based data cache.
 *
 * Pages used by the page cache can be in one of the following states:
 *
 *  - ALLOCATED: Currently in use (memory mapped, or being used by an I/O
 *    operation), or unused but there is no backing source for the cache (e.g.
 *    ramfs).
 *
 *    The page reference count is used to track the number of users.
 *    get_cache_page() increments the count when it returns a page,
 *    release_cache_page() decrements it. For caches with a backing source,
 *    the page is moved to a CACHED_* state while the count is 0.
 *
 *  - CACHED_CLEAN: Currently unused, with no modifications.
 *
 *  - CACHED_DIRTY: Currently unused, with modifications that need to be written
 *    back to the source.
 *
 * Pages in either of the unused states are available to page maintenance
 * operations:
 *
 *  - Writeback: The page writer thread periodically flushes CACHED_DIRTY
 *    pages back to the source.
 *
 *  - Reclaim: When the system is low on memory, the page allocator can evict
 *    CACHED_CLEAN pages to make them available for other users.
 *
 * Synchronisation is needed to make sure that when a maintenance operation
 * selects a page, its cache will not attempt to use it at the same time, and
 * also that its cache will not be destroyed until the operation is complete.
 *
 * This synchronisation is done via the PAGE_BUSY flag. This needs to be
 * atomically set before changing the state of a page when it is currently in
 * a state visible to maintenance operations. Maintenance operations will also
 * attempt to set the flag before performing the operation, and if they cannot
 * set it, they will skip the page.
 *
 * The other use of the PAGE_BUSY flag is when a page is being initially read
 * from the cache source. We don't want to hold the cache lock around I/O
 * operations, so that other pages can be accessed while they are in progress.
 * On an initial page read, the page is inserted into the cache in the ALLOCATED
 * state, but it is set busy while reading, which prevents other threads that
 * try to get the page from getting it before the read is complete.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <io/request.h>

#include <mm/page_cache.h>
#include <mm/phys.h>
#include <mm/slab.h>

#include <assert.h>
#include <kdb.h>
#include <status.h>

/** Define to enable debug output. */
//#define DEBUG_CACHE

#ifdef DEBUG_CACHE
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/**
 * Per-page data structure. This contains tracking information for each cached
 * page. These used to be a part of page_t, but they were split out so that we
 * don't need to take up space in every page_t for things that are only needed
 * when a page is used by the page cache.
 */
typedef struct page_cache_entry {
    /** Link to cache pages tree. Key is used to get page offset. */
    avl_tree_node_t link;

    page_cache_t *cache;            /**< Owning cache. */
    page_t *page;                   /**< Allocated page. */
} page_cache_entry_t;

/** Behaviour flags for getting a cache page. */
enum {
    /** Map the page into memory. */
    PAGE_CACHE_GET_PAGE_MAP             = (1<<0),

    /** Interruptible operation. */
    PAGE_CACHE_GET_PAGE_INTERRUPTIBLE   = (1<<1),
};

/** Details of a cached page returned from get_cache_page(). */
typedef struct page_cache_page_handle {
    page_cache_entry_t *entry;
    void *mapping;

    /** Set by the user to whether the page is dirty. */
    bool dirty;
} page_cache_page_handle_t;

/** Wait tracking for busy pages. */
typedef struct page_cache_waiter {
    list_t link;
    thread_t *thread;
    page_cache_entry_t *entry;
} page_cache_waiter_t;

/** Slab cache for allocating page_cache_t. */
static slab_cache_t *page_cache_cache;

/** Slab cache for allocating page_cache_entry_t. */
static slab_cache_t *page_cache_entry_cache;

static inline bool page_is_unused(page_t *page) {
    return refcount_get(&page->count) == 0;
}

static void page_cache_ctor(void *obj, void *data) {
    page_cache_t *cache = obj;

    mutex_init(&cache->lock, "page_cache_lock", 0);
    avl_tree_init(&cache->pages);
    list_init(&cache->waiters);
    spinlock_init(&cache->waiters_lock, "page_cache_waiters_lock");
}

static page_cache_entry_t *alloc_cache_page(page_cache_t *cache, uint32_t mmflag) {
    /* These are always allocated with MM_KERNEL. */
    page_cache_entry_t *entry = slab_cache_alloc(page_cache_entry_cache, MM_KERNEL);

    entry->page = page_alloc(mmflag);
    if (!entry->page) {
        slab_cache_free(page_cache_entry_cache, entry);
        return NULL;
    }

    entry->page->cache_entry = entry;
    entry->cache             = cache;

    return entry;
}

static void free_cache_page(page_cache_entry_t *entry) {
    page_free(entry->page);
    slab_cache_free(page_cache_entry_cache, entry);
}

/**
 * Waits for a busy page to become unbusy. The cache must be locked on entry,
 * and it will be unlocked and relocked around the wait, so the caller must
 * handle changes to the cache while waiting.
 *
 * The page can be evicted while waiting, this function will return an error if
 * that happens and the caller must attempt to look up their page again.
 *
 * @param flags         Behaviour flags (PAGE_CACHE_GET_PAGE_*).
 *
 * @return              STATUS_SUCCESS if the page became unbusy.
 *                      STATUS_TRY_AGAIN if the page was evicted.
 *                      STATUS_INTERRUPTED if the wait was interrupted.
 */
static status_t wait_for_unbusy_cache_page(page_cache_t *cache, page_cache_entry_t *entry, uint32_t flags) {
    page_cache_waiter_t waiter;

    waiter.thread = curr_thread;
    waiter.entry  = entry;

    list_init(&waiter.link);
    list_append(&cache->waiters, &waiter.link);

    /* The spinlock is only needed to ensure that another thread cannot attempt
     * to wake the thread before it has gone to sleep, in which case the wakeup
     * would be missed. The list is protected by the cache lock. */
    spinlock_lock(&cache->waiters_lock);
    mutex_unlock(&cache->lock);

    uint32_t sleep_flags = __SLEEP_NO_RELOCK;
    if (flags & PAGE_CACHE_GET_PAGE_INTERRUPTIBLE)
        sleep_flags |= SLEEP_INTERRUPTIBLE;

    status_t ret = thread_sleep(&cache->waiters_lock, -1, "page_cache_waiters", sleep_flags);

    mutex_lock(&cache->lock);

    /* Still on the list on interrupt. */
    list_remove(&waiter.link);

    if (ret != STATUS_SUCCESS) {
        assert(ret == STATUS_INTERRUPTED);
        return STATUS_INTERRUPTED;
    } else if (waiter.entry == NULL) {
        return STATUS_TRY_AGAIN;
    } else {
        return STATUS_SUCCESS;
    }
}

/** Tries to set a cache page busy.
 * @return          Whether successful. */
static inline bool try_busy_cache_page(page_cache_entry_t *entry) {
    return !(page_set_flag(entry->page, PAGE_BUSY) & PAGE_BUSY);
}

/** Makes a cache page busy, waiting until it can be set.
 * @see             wait_for_unbusy_cache_page(). */
static status_t busy_cache_page(page_cache_t *cache, page_cache_entry_t *entry, uint32_t flags) {
    while (page_set_flag(entry->page, PAGE_BUSY) & PAGE_BUSY) {
        status_t ret = wait_for_unbusy_cache_page(cache, entry, 0);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/**
 * Set a cache page as unbusy and wake any threads waiting for it. This must
 * be called with the cache lock held, and the caller must have made the page
 * busy in the first place.
 *
 * @param evicted       If true, the page is being evicted so the caller must
 *                      not continue to use it.
 */
static void unbusy_cache_page(page_cache_t *cache, page_cache_entry_t *entry, bool evicted) {
    uint16_t prev __unused = page_clear_flag(entry->page, PAGE_BUSY);
    assert(prev & PAGE_BUSY);

    list_foreach_safe(&cache->waiters, iter) {
        page_cache_waiter_t *waiter = list_entry(iter, page_cache_waiter_t, link);

        if (waiter->entry == entry) {
            if (evicted)
                waiter->entry = NULL;

            list_remove(&waiter->link);

            spinlock_lock(&cache->waiters_lock);
            thread_wake(waiter->thread);
            spinlock_unlock(&cache->waiters_lock);
        }
    }
}

/**
 * Evicts a page from a cache. The cache must be locked, and the caller must
 * have marked the page busy.
 */
static void evict_cache_page(page_cache_t *cache, page_cache_entry_t *entry) {
    assert(refcount_get(&entry->page->count) == 0);

    avl_tree_remove(&cache->pages, &entry->link);

    /* Wake any waiters and tell them it is invalid. */
    unbusy_cache_page(cache, entry, true);

    free_cache_page(entry);
}

static status_t get_existing_cache_page(page_cache_t *cache, page_cache_entry_t *entry, uint32_t flags) {
    page_t *page = entry->page;

    /* This loops either until we succesfully acquire this page, or it is
     * evicted and we must restart the outer loop in get_cache_page(). */
    while (true) {
        bool was_busy;
        if (page->state != PAGE_STATE_ALLOCATED) {
            assert(page->state == PAGE_STATE_CACHED_CLEAN || page->state == PAGE_STATE_CACHED_DIRTY);
            assert(page_is_unused(page));

            /* Page is currently in an unused state, we need to transition it
             * to allocated. In unused states it is available to maintenance
             * operations, so we must atomically make it busy in order to
             * transition. If it is already busy, a maintenance operation has
             * picked it up. */
            was_busy = !try_busy_cache_page(entry);
            if (!was_busy) {
                page_set_state(page, PAGE_STATE_ALLOCATED);

                /* Page is now good to go, just clear busy (no need to wake
                 * waiters, we didn't release the lock so there won't be any). */
                page_clear_flag(page, PAGE_BUSY);
            }
        } else {
            /* Page is allocated, but it could still be busy if it is a new
             * page that another thread is reading in (see get_new_cache_page()). */
            was_busy = page_flags(page) & PAGE_BUSY;
        }

        if (was_busy) {
            status_t ret = wait_for_unbusy_cache_page(cache, entry, flags);
            if (ret == STATUS_INTERRUPTED || ret == STATUS_TRY_AGAIN) {
                /* Interrupted (fail), or page is no longer valid (restart the
                 * outer loop). */
                return ret;
            }

            assert(ret == STATUS_SUCCESS);

            /* Loop again to re-test the state, it could have changed while
             * waiting. */
        } else {
            /* Ready to go. */
            refcount_inc(&page->count);

            dprintf(
                "page_cache: retreived existing page 0x%" PRIxPHYS " from offset 0x%" PRIx64 " in %p\n",
                page->addr, entry->link.key, cache);

            return STATUS_SUCCESS;
        }
    }
}

static status_t get_new_cache_page(
    page_cache_t *cache, offset_t offset, uint32_t flags,
    page_cache_page_handle_t *handle)
{
    /* Allocate a new page. */
    page_cache_entry_t *entry = alloc_cache_page(cache, MM_KERNEL);

    page_t *page = entry->page;

    /* Add the page to the cache. */
    refcount_inc(&page->count);
    avl_tree_insert(&cache->pages, offset, &entry->link);

    void *mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);

    /* If the cache has a backing source, read in data, else zero the page. */
    if (cache->ops) {
        /* We don't want to hold the cache lock while we read data, so mark the
         * page as busy. This will allow other cache users to pick up this page
         * since we have put it in the cache, but they will wait until the read
         * is complete before they can use it. */
        page_set_flag(page, PAGE_BUSY);

        mutex_unlock(&cache->lock);
        status_t ret = cache->ops->read_page(cache, mapping, offset);
        mutex_lock(&cache->lock);

        /* Nobody else should succeed in getting the page until we wake them. */
        assert(page->state == PAGE_STATE_ALLOCATED);
        assert(refcount_get(&page->count) == 1);

        if (ret != STATUS_SUCCESS) {
            phys_unmap(mapping, PAGE_SIZE);

            /* Other users may have picked this up and are waiting for it to be
             * ready, so have to go through the full eviction path. */
            refcount_dec(&page->count);
            evict_cache_page(cache, entry);
            return ret;
        }

        /* Wake anyone who was waiting for our read. */
        unbusy_cache_page(cache, entry, false);
    } else {
        // TODO: We could optimise this to use a system-wide zero page if this
        // is going to be a read-only mapping.
        memset(mapping, 0, PAGE_SIZE);
    }

    dprintf(
        "page_cache: cached new page 0x%" PRIxPHYS " at offset 0x%" PRIx64 " in %p\n",
        page->addr, offset, cache);

    handle->entry   = entry;
    handle->mapping = mapping;

    return STATUS_SUCCESS;
}

/**
 * Gets a page from a cache. If the page already exists in the cache, it will
 * be returned, otherwise a new page will be read in.
 *
 * The cache must be locked on entry. It can be unlocked and relocked while
 * waiting for pages or performing I/O.
 *
 * @param cache         Cache to get page from (locked).
 * @param offset        Offset of page to get.
 * @param flags         Behaviour flags (PAGE_CACHE_GET_PAGE_*).
 * @param _handle       Where to store page details.
 *
 * @return              Status code describing result of the operation.
 */
static status_t get_cache_page(
    page_cache_t *cache, offset_t offset, uint32_t flags,
    page_cache_page_handle_t *handle)
{
    status_t ret;

    assert(!(offset % PAGE_SIZE));

    handle->entry   = NULL;
    handle->mapping = NULL;
    handle->dirty   = false;

    while (true) {
        /* Check whether it is within the size of the cache. */
        if (offset >= cache->size)
            return STATUS_INVALID_ADDR;

        /* Check if we have it cached. */
        handle->entry = avl_tree_lookup(&cache->pages, offset, page_cache_entry_t, link);
        if (handle->entry) {
            ret = get_existing_cache_page(cache, handle->entry, flags);
            if (ret == STATUS_TRY_AGAIN) {
                /* Page is no longer valid. */
                handle->entry = NULL;
                continue;
            }
        } else {
            ret = get_new_cache_page(cache, offset, flags, handle);
        }

        if (ret == STATUS_SUCCESS) {
            /* Get a mapping for the page if needed. May have already created a
             * mapping for reading a new page, which we reuse. */
            if (flags & PAGE_CACHE_GET_PAGE_MAP) {
                if (!handle->mapping)
                    handle->mapping = phys_map(handle->entry->page->addr, PAGE_SIZE, MM_KERNEL);
            } else if (handle->mapping) {
                phys_unmap(handle->mapping, PAGE_SIZE);
                handle->mapping = NULL;
            }
        }

        return ret;
    }
}

/**
 * Releases a cache page that was previously returned by get_cache_page(). The
 * cache must be locked. If the page was dirtied, the flag should be set in the
 * handle.
 *
 * @param cache         Cache that the page belongs to (locked).
 * @param handle        Handle to page to release.
 */
static void release_cache_page(page_cache_t *cache, page_cache_page_handle_t *handle) {
    page_cache_entry_t *entry = handle->entry;
    offset_t offset           = entry->link.key;
    page_t *page              = entry->page;

    dprintf(
        "page_cache: released page 0x%" PRIxPHYS " at offset 0x%" PRIx64 " in %p\n",
        page->addr, offset, cache);

    if (handle->mapping)
        phys_unmap(handle->mapping, PAGE_SIZE);

    assert(page->state == PAGE_STATE_ALLOCATED);
    assert(!page_is_unused(page));
    assert(!(page_flags(page) & PAGE_BUSY));

    /* Mark as dirty if requested. */
    bool dirty = handle->dirty;
    if (dirty) {
        page_set_flag(page, PAGE_DIRTY);
    } else {
        dirty = page_flags(page) & PAGE_DIRTY;
    }

    /* Decrease the reference count. */
    if (refcount_dec(&page->count) == 0) {
        /*
         * If the page is outside of the cache's size (i.e. cache has been
         * resized with pages in use), discard it.
         *
         * Otherwise, if the cache has a backing source, move the page to the
         * appropriate cached state to make it visible to maintenance
         * operations.
         *
         * This does not need the page to be marked as busy. We only need to use
         * that flag in unused states. The page cannot be busy at this point
         * either, since we would not have returned it from get_cache_page() if
         * it were.
         */
        if (offset >= cache->size) {
            avl_tree_remove(&cache->pages, &entry->link);
            free_cache_page(entry);
        } else if (cache->ops) {
            if (dirty) {
                page_set_state(page, PAGE_STATE_CACHED_DIRTY);
            } else {
                page_set_state(page, PAGE_STATE_CACHED_CLEAN);
            }
        }
    }
}

/**
 * Flushes changes to a cache page. The cache must be locked, and the caller
 * must have marked the page as busy. The page will still be busy upon return.
 */
static status_t flush_busy_cache_page(page_cache_t *cache, page_cache_entry_t *entry) {
    status_t ret;

    offset_t offset = entry->link.key;
    page_t *page    = entry->page;

    assert(page_flags(page) & PAGE_BUSY);
    assert(page->state == PAGE_STATE_CACHED_DIRTY);

    /* If the page is outside of the cache, it may be there because the cache
     * was shrunk but with the page in use - ignore this. Also ignore pages that
     * aren't dirty. */
    if (offset >= cache->size || !(page_flags(page) & PAGE_DIRTY))
        return STATUS_SUCCESS;

    /* Should only end up here if the page is writable - when releasing pages
     * the dirty flag is cleared if there is no write operation. */
    assert(cache->ops && cache->ops->write_page);

    void *mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);

    /* Page is busy, nothing else can modify this page while we're in this
     * state. */
    mutex_unlock(&cache->lock);
    ret = cache->ops->write_page(cache, mapping, offset);
    mutex_lock(&cache->lock);

    phys_unmap(mapping, PAGE_SIZE);

    if (ret == STATUS_SUCCESS) {
        page_clear_flag(page, PAGE_DIRTY);
        page_set_state(page, PAGE_STATE_CACHED_CLEAN);
    }

    return ret;
}

/**
 * Flushes changes to a page in the CACHED_DIRTY state. This must only be
 * called from the page writer. The page must have been atomically made busy.
 * The busy flag will be cleared when this returns.
 *
 * @param page          Page to flush.
 *
 * @return              Status code describing result of the operation.
 */
status_t page_cache_flush_page(page_t *page) {
    page_cache_entry_t *entry = page->cache_entry;
    page_cache_t *cache       = entry->cache;

    /* The fact that the caller successfully made this busy guarantees that the
     * cache will remain alive until we finish. page_cache_destroy() cannot
     * complete while there are busy pages. */
    mutex_lock(&cache->lock);

    status_t ret = flush_busy_cache_page(cache, entry);
    unbusy_cache_page(cache, entry, false);

    mutex_unlock(&cache->lock);
    return ret;
}

static status_t page_cache_region_get_page(vm_region_t *region, offset_t offset, page_t **_page) {
    page_cache_t *cache = region->private;

    mutex_lock(&cache->lock);

    /* Not using interruptible sleep here since doing so would open up the
     * possibility for another process to crash this one by interrupting it
     * while it's trying to map in a page. */
    page_cache_page_handle_t handle;
    status_t ret = get_cache_page(cache, offset, 0, &handle);

    mutex_unlock(&cache->lock);

    if (ret == STATUS_SUCCESS)
        *_page = handle.entry->page;

    return ret;
}

static void page_cache_region_release_page(vm_region_t *region, page_t *page) {
    page_cache_t *cache = region->private;

    /* The VM system will have already flagged the page as dirty if necessary,
     * from the page table dirty flags. */
    page_cache_page_handle_t handle = {};
    handle.entry = page->cache_entry;

    mutex_lock(&cache->lock);
    release_cache_page(cache, &handle);
    mutex_unlock(&cache->lock);
}

/** VM region operations for mapping a page cache. */
const vm_region_ops_t page_cache_region_ops = {
    .get_page     = page_cache_region_get_page,
    .release_page = page_cache_region_release_page,
};

/** Performs I/O on a cache.
 * @param cache         Cache to read from.
 * @param request       I/O request to perform.
 * @return              Status code describing result of the operation. */
status_t page_cache_io(page_cache_t *cache, io_request_t *request) {
    status_t ret = STATUS_SUCCESS;

    mutex_lock(&cache->lock);

    /* Requests from userspace use interruptible sleep. */
    uint32_t flags = PAGE_CACHE_GET_PAGE_MAP;
    if (request->target == IO_TARGET_USER)
        flags |= PAGE_CACHE_GET_PAGE_INTERRUPTIBLE;

    offset_t offset = request->offset;
    offset_t end    = offset + request->total;

    while (true) {
        /* Each iteration we must check against the current cache size, since
         * when we release the lock around I/O operations, the size can change. */
        end = min(end, cache->size);
        if (offset >= end)
            break;

        offset_t page_start  = round_down(offset, PAGE_SIZE);
        offset_t page_end    = min(end, page_start + PAGE_SIZE);
        offset_t page_offset = offset - page_start;
        offset_t page_count  = page_end - offset;

        page_cache_page_handle_t handle;
        ret = get_cache_page(cache, page_start, flags, &handle);
        if (ret != STATUS_SUCCESS)
            break;

        /* Release lock while copying to give others a chance to use the cache
         * at the same time. */
        mutex_unlock(&cache->lock);

        ret = io_request_copy(request, handle.mapping + page_offset, page_count, true);

        mutex_lock(&cache->lock);

        handle.dirty = request->op == IO_OP_WRITE;
        release_cache_page(cache, &handle);

        if (ret != STATUS_SUCCESS)
            break;

        offset += page_count;
    }

    mutex_unlock(&cache->lock);
    return STATUS_SUCCESS;
}

/** Reads data from a cache into a kernel buffer.
 * @param cache         Cache to read from.
 * @param buf           Buffer to read into.
 * @param size          Number of bytes to read.
 * @param offset        Offset to read from.
 * @param _bytes        Where to store number of bytes read.
 * @return              Status code describing result of the operation. */
status_t page_cache_read(page_cache_t *cache, void *buf, size_t size, offset_t offset, size_t *_bytes) {
    status_t ret;

    io_vec_t vec;
    vec.buffer = buf;
    vec.size   = size;

    if (_bytes)
        *_bytes = 0;

    io_request_t request;
    ret = io_request_init(&request, &vec, 1, offset, IO_OP_READ, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = page_cache_io(cache, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
    return ret;
}

/** Wrtes data to a cache from a kernel buffer.
 * @param cache         Cache to write to.
 * @param buf           Buffer to write from.
 * @param size          Number of bytes to write.
 * @param offset        Offset to write to.
 * @param _bytes        Where to store number of bytes written.
 * @return              Status code describing result of the operation. */
status_t page_cache_write(page_cache_t *cache, const void *buf, size_t size, offset_t offset, size_t *_bytes) {
    status_t ret;

    io_vec_t vec;
    vec.buffer = (void *)buf;
    vec.size   = size;

    if (_bytes)
        *_bytes = 0;

    io_request_t request;
    ret = io_request_init(&request, &vec, 1, offset, IO_OP_WRITE, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = page_cache_io(cache, &request);
    if (_bytes)
        *_bytes = request.transferred;

    io_request_destroy(&request);
    return ret;
}

/**
 * Resizes a cache. This evicts any cached pages that are outside of the new
 * size, unless they are in use, in which case they will be freed once released.
 *
 * @param cache         Cache to resize.
 * @param size          New size of the cache.
 */
void page_cache_resize(page_cache_t *cache, offset_t size) {
    mutex_lock(&cache->lock);

    if (size < cache->size) {
        /* lookup_ge_node() finds the first page greater or equal the new size. */
        for (avl_tree_node_t *iter = avl_tree_lookup_ge_node(&cache->pages, size); iter; ) {
            page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);
            offset_t offset __unused  = entry->link.key;
            page_t *page              = entry->page;

            assert(offset >= size);

            if (page_is_unused(page)) {
                assert(refcount_get(&page->count) == 0);

                /* Page must be busy to evict, wait until we can set busy. */
                // TODO: Maybe this should be interruptible at some point. We
                // would need to flush dirty pages rather than discarding them
                // in case of failure.
                status_t ret = busy_cache_page(cache, entry, 0);
                if (ret == STATUS_TRY_AGAIN) {
                    /*
                     * Eviction of the current entry means that it is no longer
                     * valid, including our current iterator. We must restart
                     * the page loop in this case.
                     *
                     * Note that we cannot save the next tree node before
                     * waiting, because waiting releases the lock and therefore
                     * that entry might also be invalid after waiting. The only
                     * entry we have any guarantee about is the current one.
                     */
                    iter = avl_tree_first(&cache->pages);
                    continue;
                }

                assert(ret == STATUS_SUCCESS);

                /* State could change while waiting. */
                if (page_is_unused(page)) {
                    /* Since the page is outside the new size, we no longer
                     * care about its data, so we just evict it without
                     * flushing. */
                    evict_cache_page(cache, entry);
                } else {
                    unbusy_cache_page(cache, entry, false);
                }
            }

            iter = avl_tree_next(iter);
        }
    }

    cache->size = size;

    mutex_unlock(&cache->lock);
}

/**
 * Flushes modifications to a cache. If a failure occurs, the function carries
 * on attempting to flush, but still returns an error. If multiple errors occur,
 * it is the most recent that is returned.
 *
 * @param cache         Cache to flush.
 *
 * @return              Status code describing result of the operation.
 */
status_t page_cache_flush(page_cache_t *cache) {
    status_t ret = STATUS_SUCCESS;

    mutex_lock(&cache->lock);

    for (avl_tree_node_t *iter = avl_tree_first(&cache->pages); iter; ) {
        page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);
        page_t *page              = entry->page;

        /* We can only flush unused pages for now. Eventually we should make it
         * possible to flush mapped pages. */
        if (page->state == PAGE_STATE_CACHED_DIRTY) {
            /* Page must be busy to flush, wait until we can set busy. */
            // TODO: Maybe this should be interruptible at some point.
            status_t err = busy_cache_page(cache, entry, 0);
            if (err == STATUS_TRY_AGAIN) {
                /* Same as page_cache_resize(), when evicted we must restart
                 * the loop. */
                iter = avl_tree_first(&cache->pages);
                continue;
            }

            assert(err == STATUS_SUCCESS);

            /* Could change while waiting. */
            if (page->state == PAGE_STATE_CACHED_DIRTY) {
                err = flush_busy_cache_page(cache, entry);
                if (err != STATUS_SUCCESS)
                    ret = err;
            }

            unbusy_cache_page(cache, entry, false);
        }

        iter = avl_tree_next(iter);
    }

    mutex_unlock(&cache->lock);
    return ret;
}

/** Allocates a new page cache.
 * @param size          Size of the cache.
 * @param ops           Pointer to operations structure (optional).
 * @param private       Implementation-specific data pointer.
 * @return              Pointer to cache structure. */
page_cache_t *page_cache_create(offset_t size, const page_cache_ops_t *ops, void *private) {
    page_cache_t *cache = slab_cache_alloc(page_cache_cache, MM_KERNEL);

    assert(!ops || (ops->read_page && ops->write_page));

    cache->size    = size;
    cache->ops     = ops;
    cache->private = private;

    return cache;
}

/**
 * Destroys a cache. The cache must not be in use - use only in handle close
 * functions, for example.
 *
 * This flushes all modifications. If there are any failures in writing
 * modifications, the cache will still be destroyed and data that couldn't be
 * flushed will be lost, unless there is a parent cache that this is being
 * flushed to. If it is desired to ensure that all data is written, do an
 * explicit page_cache_flush() and handle any errors before page_cache_destroy().
 *
 * @param cache         Cache to destroy. Must not be in use.
 *
 * @return              Status code describing result of the operation. If
 *                      multiple errors occur, it is the most recent that is
 *                      returned. Cache is still destroyed on error.
 */
status_t page_cache_destroy(page_cache_t *cache) {
    status_t ret = STATUS_SUCCESS;

    mutex_lock(&cache->lock);

    while (!avl_tree_empty(&cache->pages)) {
        /* Go from the root, it's quicker than descending the tree to get the
         * left-most node each time. */
        page_cache_entry_t *entry = avl_tree_entry(cache->pages.root, page_cache_entry_t, link);
        page_t *page = entry->page;

        assert(page_is_unused(page));

        /* Make the page busy to take it away from maintenance operations. */
        status_t err = busy_cache_page(cache, entry, 0);
        if (err == STATUS_TRY_AGAIN) {
            /* Maintenance operation must have evicted, entry is no longer valid
             * so restart. */
            continue;
        }

        assert(err == STATUS_SUCCESS);

        if (page->state == PAGE_STATE_CACHED_DIRTY) {
            err = flush_busy_cache_page(cache, entry);
            if (err != STATUS_SUCCESS)
                ret = err;
        }

        /* Don't unset busy, it will be done when freeing the page and must be
         * set up until that point to stop a maintenance operation picking it
         * up. There should be no waiters since the cache is not in use. */
        evict_cache_page(cache, entry);
    }

    assert(list_empty(&cache->waiters));

    mutex_unlock(&cache->lock);

    slab_cache_free(page_cache_cache, cache);
    return ret;
}

static kdb_status_t kdb_cmd_page_cache(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s <address>\n\n", argv[0]);

        kdb_printf("Prints information about a page cache.\n");
        return KDB_SUCCESS;
    } else if (argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    /* Get the address. */
    uint64_t addr;
    if (kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    page_cache_t *cache = (page_cache_t *)((ptr_t)addr);

    /* Print out basic information. */
    kdb_printf("Cache %p\n", cache);
    kdb_printf("=================================================\n");

    kdb_printf(
        "locked:  %d (%" PRId32 ")\n",
        atomic_load(&cache->lock.value), (cache->lock.holder) ? cache->lock.holder->id : -1);
    kdb_printf("size:    %" PRIu64 "\n", cache->size);
    kdb_printf("ops:     %p\n", cache->ops);
    kdb_printf("private: %p\n", cache->private);

    /* Show all cached pages. */
    kdb_printf("Cached pages:\n");
    avl_tree_foreach(&cache->pages, iter) {
        page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);

        kdb_printf(
            "  0x%016" PRIxPHYS " - Offset: %-10" PRIu64 " Flags: 0x%-4x Count: %d\n",
            entry->page->addr, entry->link.key, page_flags(entry->page),
            refcount_get(&entry->page->count));
    }

    return KDB_SUCCESS;
}

__init_text void page_cache_init(void) {
    page_cache_cache = object_cache_create(
        "page_cache_cache",
        page_cache_t, page_cache_ctor, NULL, NULL, 0, MM_BOOT);
    page_cache_entry_cache = object_cache_create(
        "page_cache_entry_cache",
        page_cache_entry_t, NULL, NULL, NULL, 0, MM_BOOT);

    kdb_register_command("page_cache", "Print information about a page cache.", kdb_cmd_page_cache);
}
