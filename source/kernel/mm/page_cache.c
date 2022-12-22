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
 * TODO:
 *  - Implement nonblocking I/O?
 *  - Maybe a hash table or a multi-level array would be more appropriate than
 *    an AVL tree for the page tree.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <io/request.h>

#include <mm/page_cache.h>
#include <mm/phys.h>
#include <mm/slab.h>

#include <proc/thread.h>

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

/** Slab cache for allocating page_cache_t. */
static slab_cache_t *page_cache_cache;

/** Slab cache for allocating page_cache_entry_t. */
static slab_cache_t *page_cache_entry_cache;

static void page_cache_ctor(void *obj, void *data) {
    page_cache_t *cache = obj;

    mutex_init(&cache->lock, "page_cache_lock", 0);
    avl_tree_init(&cache->pages);
}

static page_cache_entry_t *alloc_cache_page(page_cache_t *cache, unsigned mmflag) {
    /* These are always allocated with MM_KERNEL. */
    page_cache_entry_t *entry = slab_cache_alloc(page_cache_entry_cache, MM_KERNEL);

    entry->page = page_alloc(mmflag);
    if (!entry->page) {
        slab_cache_free(page_cache_entry_cache, entry);
        return NULL;
    }

    entry->page->private = entry;
    entry->cache         = cache;

    return entry;
}

static void free_cache_page(page_cache_entry_t *entry) {
    page_free(entry->page);
    slab_cache_free(page_cache_entry_cache, entry);
}

/** Get a page from a cache.
 * @note                Should not be passed both _mapping and _page.
 * @param cache         Cache to get page from.
 * @param offset        Offset of page to get.
 * @param overwrite     If true, then the page's data will not be read in if
 *                      it is not in the cache, a page will only be allocated.
 *                      This is used if the page is about to be overwritten.
 * @param _page         Where to store pointer to page structure.
 * @param _mapping      Where to store address of virtual mapping. If this is
 *                      set the calling thread will be wired to its CPU when
 *                      the function returns.
 * @param _shared       Where to store value stating whether a mapping had to
 *                      be shared. Only used if _mapping is set.
 * @return              Status code describing result of the operation. */
static status_t get_cache_page(
    page_cache_t *cache, offset_t offset, bool overwrite, page_t **_page,
    void **_mapping, bool *_shared)
{
    status_t ret;

    assert((_page && !_mapping) || (_mapping && !_page));
    assert(!(offset % PAGE_SIZE));

    mutex_lock(&cache->lock);

    assert(!cache->deleted);

    /* Check whether it is within the size of the cache. */
    if (offset >= cache->size) {
        mutex_unlock(&cache->lock);
        return STATUS_INVALID_ADDR;
    }

    /* Check if we have it cached. */
    page_cache_entry_t *entry = avl_tree_lookup(&cache->pages, offset, page_cache_entry_t, link);
    if (entry) {
        if (refcount_inc(&entry->page->count) == 1)
            page_set_state(entry->page, PAGE_STATE_ALLOCATED);

        mutex_unlock(&cache->lock);

        /* Map it in if required. Wire the thread to the current CPU and specify
         * that the mapping is not being shared - the mapping will only be
         * accessed by this thread, so we can save having to do a remote TLB
         * invalidation. */
        if (_mapping) {
            assert(_shared);

            thread_wire(curr_thread);

            *_mapping = phys_map(entry->page->addr, PAGE_SIZE, MM_KERNEL);
            *_shared  = false;
        } else {
            *_page = entry->page;
        }

        dprintf(
            "cache: retreived cached page 0x%" PRIxPHYS " from offset 0x%" PRIx64 " in %p\n",
            entry->page->addr, offset, cache);

        return STATUS_SUCCESS;
    }

    /* Allocate a new page. */
    entry = alloc_cache_page(cache, MM_KERNEL);

    /* Only bother filling the page with data if it's not going to be
     * immediately overwritten. */
    void *mapping = NULL;
    bool shared   = false;
    if (!overwrite) {
        /* If a read operation is provided, read in data, else zero the page. */
        if (cache->ops && cache->ops->read_page) {
            /* When reading in page data we cannot guarantee that the mapping
             * won't be shared, because it's possible that a device driver will
             * do work in another thread, which may be on another CPU. */
            mapping = phys_map(entry->page->addr, PAGE_SIZE, MM_KERNEL);
            shared  = true;

            ret = cache->ops->read_page(cache, mapping, offset);
            if (ret != STATUS_SUCCESS) {
                phys_unmap(mapping, PAGE_SIZE, true);
                free_cache_page(entry);
                mutex_unlock(&cache->lock);
                return ret;
            }
        } else {
            thread_wire(curr_thread);
            mapping = phys_map(entry->page->addr, PAGE_SIZE, MM_KERNEL);
            memset(mapping, 0, PAGE_SIZE);
        }
    }

    refcount_inc(&entry->page->count);
    avl_tree_insert(&cache->pages, offset, &entry->link);

    dprintf(
        "cache: cached new page 0x%" PRIxPHYS " at offset 0x%" PRIx64 " in %p\n",
        entry->page->addr, offset, cache);

    /* It's safe to access the entry beyond here because of the reference added. */
    mutex_unlock(&cache->lock);

    if (_mapping) {
        assert(_shared);

        /* Reuse any mapping that may have already been created. */
        if (!mapping) {
            thread_wire(curr_thread);
            mapping = phys_map(entry->page->addr, PAGE_SIZE, MM_KERNEL);
        }

        *_mapping = mapping;
        *_shared  = shared;
    } else {
        /* Page mapping is not required, get rid of it. */
        if (mapping) {
            phys_unmap(mapping, PAGE_SIZE, shared);
            if (!shared)
                thread_unwire(curr_thread);
        }

        *_page = entry->page;
    }

    return STATUS_SUCCESS;
}

/** Releases a page from a cache.
 * @param cache         Cache that the page belongs to. Must be locked.
 * @param entry         Page to release.
 * @param dirty         Whether the page has been dirtied. */
static void release_cache_page(page_cache_t *cache, page_cache_entry_t *entry, bool dirty) {
    offset_t offset = entry->link.key;

    dprintf(
        "cache: released page 0x%" PRIxPHYS " at offset 0x%" PRIx64 " in %p\n",
        entry->page->addr, offset, cache);

    /* Mark as modified if requested. */
    if (dirty) {
        page_set_flag(entry->page, PAGE_FLAG_DIRTY);
    } else {
        dirty = page_flags(entry->page) & PAGE_FLAG_DIRTY;
    }

    /* Decrease the reference count. */
    if (refcount_dec(&entry->page->count) == 0) {
        /* If the page is outside of the cache's size (i.e. cache has been
         * resized with pages in use, discard it). Otherwise, move the page to
         * the appropriate queue. */
        if (offset >= cache->size) {
            avl_tree_remove(&cache->pages, &entry->link);
            free_cache_page(entry);
        } else if (dirty && cache->ops && cache->ops->write_page) {
            page_set_state(entry->page, PAGE_STATE_CACHED_DIRTY);
        } else {
            page_clear_flag(entry->page, PAGE_FLAG_DIRTY);
            page_set_state(entry->page, PAGE_STATE_CACHED_CLEAN);
        }
    }
}

/** Flushes changes to a cache page.
 * @param cache         Cache that the page belongs to. Must be locked.
 * @param entry         Page to flush. */
static status_t flush_cache_page(page_cache_t *cache, page_cache_entry_t *entry) {
    offset_t offset = entry->link.key;

    /* If the page is outside of the cache, it may be there because the cache
     * was shrunk but with the page in use. Ignore this. Also ignore pages that
     * aren't dirty. */
    if (offset >= cache->size || !(page_flags(entry->page) & PAGE_FLAG_DIRTY))
        return STATUS_SUCCESS;

    /* Should only end up here if the page is writable - when releasing pages
     * the dirty flag is cleared if there is no write operation. */
    assert(cache->ops && cache->ops->write_page);

    void *mapping = phys_map(entry->page->addr, PAGE_SIZE, MM_KERNEL);

    status_t ret = cache->ops->write_page(cache, mapping, offset);
    if (ret == STATUS_SUCCESS) {
        /* Clear dirty flag only if the page reference count is zero. This is
         * because the page may be mapped into an address space as read-write. */
        if (refcount_get(&entry->page->count) == 0) {
            page_clear_flag(entry->page, PAGE_FLAG_DIRTY);
            page_set_state(entry->page, PAGE_STATE_CACHED_CLEAN); 
        }
    }

    phys_unmap(mapping, PAGE_SIZE, true);
    return ret;
}

/** Get and map a page from a cache.
 * @param cache         Cache to get page from.
 * @param offset        Offset of page to get.
 * @param overwrite     If true, then the page's data will not be read in if
 *                      it is not in the cache, a page will only be allocated.
 *                      This is used if the page is about to be overwritten.
 * @param _addr         Where to store address of mapping.
 * @param _shared       Where to store value stating whether a mapping had to
 *                      be shared.
 * @return              Status code describing result of the operation. */
static status_t map_cache_page(
    page_cache_t *cache, offset_t offset, bool overwrite, void **_addr,
    bool *_shared)
{
    assert(_addr && _shared);

    return get_cache_page(cache, offset, overwrite, NULL, _addr, _shared);
}

/** Unmap and release a page from a cache.
 * @param cache         Cache to release page in.
 * @param addr          Address of mapping.
 * @param offset        Offset of page to release.
 * @param dirty         Whether the page has been dirtied.
 * @param shared        Shared value returned from map_cache_page(). */
static void unmap_cache_page(
    page_cache_t *cache, void *mapping, offset_t offset, bool dirty,
    bool shared)
{
    phys_unmap(mapping, PAGE_SIZE, shared);
    if (!shared)
        thread_unwire(curr_thread);

    mutex_lock(&cache->lock);

    page_cache_entry_t *entry = avl_tree_lookup(&cache->pages, offset, page_cache_entry_t, link);
    if (unlikely(!entry))
        fatal("Tried to release page that isn't cached");

    release_cache_page(cache, entry, dirty);

    mutex_unlock(&cache->lock);
}

/** Flush changes to a page the CACHED_DIRTY state.
 * @param page          Page to flush.
 * @return              Status code describing result of the operation. */
status_t page_cache_flush_page(page_t *page) {
    /* Must be careful - another thread could be destroying the cache.
     * FIXME: wut? */
    page_cache_entry_t *entry = page->private;
    page_cache_t *cache       = entry->cache;

    mutex_lock(&cache->lock);

    if (cache->deleted) {
        mutex_unlock(&cache->lock);
        return true;
    }

    status_t ret = flush_cache_page(cache, entry);
    mutex_unlock(&cache->lock);
    return (ret == STATUS_SUCCESS);
}

static status_t page_cache_region_get_page(vm_region_t *region, offset_t offset, page_t **_page) {
    return get_cache_page(region->private, offset, false, _page, NULL, NULL);
}

static void page_cache_region_release_page(vm_region_t *region, page_t *page) {
    page_cache_t *cache       = region->private;
    page_cache_entry_t *entry = page->private;

    mutex_lock(&cache->lock);

    /* The VM system will have flagged the page as dirty if necessary. */
    release_cache_page(cache, entry, false);

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
    status_t ret;

    mutex_lock(&cache->lock);

    /* Ensure that we do not go past the end of the cache. */
    if (request->offset >= cache->size || !request->total) {
        mutex_unlock(&cache->lock);
        return STATUS_SUCCESS;
    }

    size_t total = ((offset_t)(request->offset + request->total) > cache->size)
        ? (size_t)(cache->size - request->offset)
        : request->total;

    mutex_unlock(&cache->lock);

    /* Now work out the start page and the end page. Subtract one from count to
     * prevent end from going onto the next page when the offset plus the count
     * is an exact multiple of PAGE_SIZE. */
    offset_t start = round_down(request->offset, PAGE_SIZE);
    offset_t end   = round_down((request->offset + (total - 1)), PAGE_SIZE);

    void *mapping;
    bool shared;

    /* If we're not starting on a page boundary, we need to do a partial
     * transfer on the initial page to get us up to a page boundary. If the
     * transfer only goes across one page, this will handle it. */
    if (request->offset % PAGE_SIZE) {
        ret = map_cache_page(cache, start, false, &mapping, &shared);
        if (ret != STATUS_SUCCESS)
            return ret;

        size_t count = (start != end)
            ? (size_t)(PAGE_SIZE - (request->offset % PAGE_SIZE))
            : total;

        ret = io_request_copy(request, mapping + (request->offset % PAGE_SIZE), count, true);

        // TODO: This is looking up again unnecessarily
        unmap_cache_page(cache, mapping, start, false, shared);

        if (ret != STATUS_SUCCESS)
            return ret;

        total -= count;
        start += PAGE_SIZE;
    }

    /* Handle any full pages. */
    while (total >= PAGE_SIZE) {
        /* For writes, we pass the overwrite parameter as true to
         * map_cache_page() here, so that if the page is not in the cache, its
         * data will not be read in - we're about to overwrite it, so it would
         * not be necessary. */
        ret = map_cache_page(cache, start, request->op == IO_OP_WRITE, &mapping, &shared);
        if (ret != STATUS_SUCCESS)
            return ret;

        ret = io_request_copy(request, mapping, PAGE_SIZE, true);
        unmap_cache_page(cache, mapping, start, false, shared);

        if (ret != STATUS_SUCCESS)
            return ret;

        total -= PAGE_SIZE;
        start += PAGE_SIZE;
    }

    /* Handle anything that's left. */
    if (total) {
        ret = map_cache_page(cache, start, false, &mapping, &shared);
        if (ret != STATUS_SUCCESS)
            return ret;

        ret = io_request_copy(request, mapping, total, true);
        unmap_cache_page(cache, mapping, start, false, shared);

        if (ret != STATUS_SUCCESS)
            return ret;
    }

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

/** Resizes a cache.
 * @param cache         Cache to resize.
 * @param size          New size of the cache. */
void page_cache_resize(page_cache_t *cache, offset_t size) {
    mutex_lock(&cache->lock);

    /* Shrink the cache if the new size is smaller. If any pages are in use they
     * will get freed once they are released. */
    if (size < cache->size) {
        avl_tree_foreach_safe(&cache->pages, iter) {
            page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);

            offset_t offset = entry->link.key;

            if (offset >= size && refcount_get(&entry->page->count) == 0) {
                avl_tree_remove(&cache->pages, &entry->link);
                free_cache_page(entry);
            }
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

    /* Flush all pages. */
    avl_tree_foreach(&cache->pages, iter) {
        page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);

        status_t err = flush_cache_page(cache, entry);
        if (err != STATUS_SUCCESS)
            ret = err;
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

    cache->size    = size;
    cache->ops     = ops;
    cache->private = private;
    cache->deleted = false;

    return cache;
}

/** Destroys a cache.
 * @param cache         Cache to destroy. Should NOT be in use.
 * @param discard       Whether to discard modifications. The function will
 *                      always succeed if true.
 * @return              Status code describing result of the operation. */
status_t page_cache_destroy(page_cache_t *cache, bool discard) {
    mutex_lock(&cache->lock);

    cache->deleted = true;

    /* Free all pages. */
    avl_tree_foreach_safe(&cache->pages, iter) {
        page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);

        if (refcount_get(&entry->page->count) != 0) {
            fatal("Cache page still in use while destroying");
        } else if (!discard) {
            status_t ret = flush_cache_page(cache, entry);
            if (ret != STATUS_SUCCESS) {
                cache->deleted = false;
                mutex_unlock(&cache->lock);
                return ret;
            }
        }

        avl_tree_remove(&cache->pages, &entry->link);
        free_cache_page(entry);
    }

    /* Unlock and relock the cache to allow any attempts to flush or evict a
     * page see the deleted flag. */
    mutex_unlock(&cache->lock);
    mutex_lock(&cache->lock);
    mutex_unlock(&cache->lock);

    slab_cache_free(page_cache_cache, cache);
    return STATUS_SUCCESS;
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
    kdb_printf("deleted: %d\n\n", cache->deleted);

    /* Show all cached pages. */
    kdb_printf("Cached pages:\n");
    avl_tree_foreach(&cache->pages, iter) {
        page_cache_entry_t *entry = avl_tree_entry(iter, page_cache_entry_t, link);

        kdb_printf(
            "  Page 0x%016" PRIxPHYS " - Offset: %-10" PRIu64 " Flags: 0x%-4x Count: %d\n",
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
