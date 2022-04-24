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
 * @brief               Virtual memory manager.
 *
 * Reference:
 * - The UVM Virtual Memory System.
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.28.1236
 *
 * Parts of the design of the VMM are inspired by NetBSD's UVM (although not
 * the same as), in particular the implementation of anonymous memory and
 * copy-on-write.
 *
 * Some details on the various region trees/lists and the method used for
 * allocation of free regions.
 *  - There is a vm_region_t structure for each region of memory within an
 *    address space, whether allocated or not.
 *  - There is an AVL tree containing only allocated regions, used for fast
 *    region lookups upon page faults. We do not care about free or reserved
 *    regions when doing these lookups, as a page fault on a free region is
 *    invalid, so including free regions in this tree would be an unnecessary
 *    bottleneck.
 *  - Free regions are attached to power of two free lists to allow fast
 *    allocation of free space for non-fixed mappings.
 *  - There is a sorted list of all regions in an address space. This is used
 *    on unmap operations to be able to find all the regions that the unmap
 *    covers.
 *
 * A brief note about reference counting for pages in the anonymous memory
 * layer:
 *  - The reference count in the page structure is used to track how many
 *    anonymous objects refer to a single page (i.e. object has been duplicated
 *    but the page has not been copied, because no write fault has occurred).
 *    If, when a write fault occurs on a page, the page structure reference
 *    count is greater than 1, the page is copied. Otherwise, the page is just
 *    remapped as read-write (if the region is VM_ACCESS_WRITE, that is).
 *  - Each object also contains an array of reference counts for each page that
 *    the object can cover. This array is used to track how many regions are
 *    mapping each page of the object, allowing pages to be freed when no more
 *    regions refer to them.
 *
 * TODO:
 *  - The anonymous object page array could be changed into a two-level array,
 *    which would reduce memory consumption for large, sparsely-used objects.
 *  - Swap support.
 *  - Implement VM_MAP_OVERCOMMIT (at the moment we just overcommit regardless).
 *  - Proper memory locking. Note that when eviction gets implemented we need to
 *    make sure that the userspace locking APIs cannot unlock any locks that the
 *    kernel makes, as the kernel locking functions make the guarantee that the
 *    locked range is safe to access from kernel mode.
 *  - Shouldn't use MM_KERNEL for user address spaces? Note that MM_USER will at
 *    some point use interruptible sleep so interrupting a process waiting for
 *    memory leaves us no choice but to crash the thread.
 */

#include <arch/frame.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/phys.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>
#include <mm/vm_cache.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <kdb.h>
#include <setjmp.h>
#include <smp.h>
#include <status.h>

/** Define to enable (very) verbose debug output. */
//#define DEBUG_VM

#ifdef DEBUG_VM
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/* This allows for using the VM flags in MMU functions without conversion. */
static_assert(VM_ACCESS_READ    == MMU_ACCESS_READ,    "Mismatched VM/MMU flag definitions");
static_assert(VM_ACCESS_WRITE   == MMU_ACCESS_WRITE,   "Mismatched VM/MMU flag definitions");
static_assert(VM_ACCESS_EXECUTE == MMU_ACCESS_EXECUTE, "Mismatched VM/MMU flag definitions");

/** Slab caches used for VM structures. */
static slab_cache_t *vm_aspace_cache;
static slab_cache_t *vm_region_cache;
static slab_cache_t *vm_amap_cache;

static void vm_aspace_ctor(void *obj, void *data) {
    vm_aspace_t *as = obj;

    mutex_init(&as->lock, "vm_aspace_lock", 0);
    refcount_set(&as->count, 0);
    avl_tree_init(&as->tree);
    list_init(&as->regions);

    for (unsigned i = 0; i < VM_FREELISTS; i++)
        list_init(&as->free[i]);
}

/** Constructor for region objects. */
static void vm_region_ctor(void *obj, void *data) {
    vm_region_t *region = obj;

    list_init(&region->header);
    list_init(&region->free_link);
    condvar_init(&region->waiters, "vm_region_waiters");

    region->locked = 0;
}

static void vm_amap_ctor(void *obj, void *data) {
    vm_amap_t *map = obj;

    mutex_init(&map->lock, "vm_amap_lock", 0);
}

/**
 * Utility functions.
 */

static inline bool vm_aspace_fits(vm_aspace_t *as, ptr_t start, size_t size) {
    assert(!list_empty(&as->regions));

    vm_region_t *first = list_first(&as->regions, vm_region_t, header);
    vm_region_t *last  = list_last(&as->regions, vm_region_t, header);

    ptr_t as_end     = last->start + last->size - 1;
    ptr_t region_end = start + size - 1;

    return (region_end >= start && start >= first->start && region_end <= as_end);
}

static inline void vm_freelist_insert(vm_region_t *region, size_t size) {
    unsigned list = highbit(size) - PAGE_WIDTH - 1;

    assert(region->state == VM_REGION_FREE);

    list_append(&region->as->free[list], &region->free_link);
    region->as->free_map |= ((ptr_t)1 << list);
}

static inline void vm_freelist_remove(vm_region_t *region) {
    unsigned list = highbit(region->size) - PAGE_WIDTH - 1;

    assert(!region->flags);

    list_remove(&region->free_link);
    if (list_empty(&region->as->free[list]))
        region->as->free_map &= ~((ptr_t)1 << list);
}

static vm_region_t *vm_region_prev(vm_region_t *region) {
    if (region == list_first(&region->as->regions, vm_region_t, header))
        return NULL;

    return list_prev(region, header);
}

static vm_region_t *vm_region_next(vm_region_t *region) {
    if (region == list_last(&region->as->regions, vm_region_t, header))
        return NULL;

    return list_next(region, header);
}

static inline bool vm_region_contains(vm_region_t *region, ptr_t addr) {
    return (addr >= region->start && addr <= (region->start + region->size - 1));
}

static inline bool vm_region_mergeable(const vm_region_t *a, const vm_region_t *b) {
    return (a->state != VM_REGION_ALLOCATED && a->state == b->state);
}

/**
 * Anonymous map functions.
 */

static vm_amap_t *vm_amap_create(size_t size) {
    assert(size);

    vm_amap_t *map = slab_cache_alloc(vm_amap_cache, MM_KERNEL);

    refcount_set(&map->count, 1);

    map->curr_size = 0;
    map->max_size  = size >> PAGE_WIDTH;
    map->pages     = kcalloc(map->max_size, sizeof(*map->pages), MM_KERNEL);
    map->rref      = kcalloc(map->max_size, sizeof(*map->rref), MM_KERNEL);

    dprintf("vm: created anonymous map %p (size: %zu, pages: %zu)\n", map,size, map->max_size);

    return map;
}

/** Clone part of an existing anonymous map. */
static vm_amap_t *vm_amap_clone(vm_amap_t *src, offset_t offset, size_t size) {
    vm_amap_t *dest = vm_amap_create(size);

    mutex_lock(&src->lock);

    size_t start = (size_t)(offset >> PAGE_WIDTH);
    size_t end   = start + (size >> PAGE_WIDTH);

    assert(end <= src->max_size);

    /* Point all of the pages in the new map to the pages from the source map:
     * they will be copied when a write fault occurs on either the source or the
     * destination. Set the region reference count for each page to 1, to
     * account for the destination region. */
    for (size_t i = start; i < end; i++) {
        if (src->pages[i]) {
            refcount_inc(&src->pages[i]->count);
            dest->curr_size++;
        }

        dest->pages[i - start] = src->pages[i];
        dest->rref[i - start]  = 1;
    }

    mutex_unlock(&src->lock);
    return dest;
}

static void vm_amap_release(vm_amap_t *map) {
    if (refcount_dec(&map->count) == 0) {
        assert(!map->curr_size);

        kfree(map->rref);
        kfree(map->pages);

        dprintf("vm: destroyed anonymous map %p\n", map);

        slab_cache_free(vm_amap_cache, map);
    }
}

/** Increase the region reference count for part of an anonymous map. */
static status_t vm_amap_map(vm_amap_t *map, offset_t offset, size_t size) {
    mutex_lock(&map->lock);

    /* Work out the entries within the object that this covers and ensure it's
     * within the object - for anonymous objects mappings can't be outside the
     * object. */
    size_t start = (size_t)(offset >> PAGE_WIDTH);
    size_t end   = start + (size >> PAGE_WIDTH);

    assert(end <= map->max_size);

    /* Increase the region reference counts for pages in the region. */
    for (size_t i = start; i < end; i++) {
        if (map->rref[i] == UINT16_MAX) {
            kprintf(LOG_DEBUG, "vm: anon object %p rref[%zu] is at maximum value!\n", map, i);

            /* Go and undo what we've done. */
            for (size_t j = start; j < i; j++)
                map->rref[j]--;

            mutex_unlock(&map->lock);
            return STATUS_NO_MEMORY;
        }

        map->rref[i]++;
    }

    mutex_unlock(&map->lock);
    return STATUS_SUCCESS;
}

/** Decrease the region reference count for part of the anonymous map. */
static void vm_amap_unmap(vm_amap_t *map, offset_t offset, size_t size) {
    mutex_lock(&map->lock);

    /* Work out the entries within the object that this covers and ensure it's
     * within the object - for anonymous objects mappings can't be outside the
     * object. */
    ptr_t start = (size_t)(offset >> PAGE_WIDTH);
    ptr_t end   = start + (size >> PAGE_WIDTH);

    assert(end <= map->max_size);

    for (size_t i = start; i < end; i++) {
        assert(map->rref[i]);

        if (--map->rref[i] == 0 && map->pages[i]) {
            dprintf(
                "vm: anon object %p rref[%zu] reached 0, freeing 0x%" PRIxPHYS "\n",
                map, i, map->pages[i]->addr);

            if (refcount_dec(&map->pages[i]->count) == 0)
                page_free(map->pages[i]);

            map->pages[i] = NULL;
            map->curr_size--;
        }
    }

    mutex_unlock(&map->lock);
}

/**
 * Page mapping functions.
 */

/** Map a page for an anonymous region into an address space.
 * @note                Address space and MMU context should be locked. */
static status_t map_anon_page(vm_region_t *region, ptr_t addr, uint32_t requested, phys_ptr_t *_phys) {
    vm_amap_t *amap = region->amap;
    page_t *page;
    status_t ret;

    /* Check if the page is already mapped. If it is and the access flags
     * include the requested acesss, we don't need to do anything. */
    phys_ptr_t phys;
    uint32_t flags;
    bool exist = mmu_context_query(region->as->mmu, addr, &phys, &flags);
    if (exist && (flags & requested) == requested) {
        if (_phys)
            *_phys = phys;

        return STATUS_SUCCESS;
    }

    /* Access flags to map with. The write flag is cleared later on if the page
     * needs to be mapped read only. */
    uint32_t access = region->access;

    /* Work out the offset into the object. */
    offset_t offset = region->amap_offset + (addr - region->start);
    size_t idx      = (size_t)(offset >> PAGE_WIDTH);

    mutex_lock(&amap->lock);

    assert(idx < amap->max_size);

    if (!amap->pages[idx] && !region->handle) {
        /* No page existing and no source. Allocate a zeroed page. */
        dprintf("vm:  anon fault: no existing page and no source, allocating new\n");
        amap->pages[idx] = page_alloc(MM_KERNEL | MM_ZERO);
        refcount_inc(&amap->pages[idx]->count);
        amap->curr_size++;
        phys = amap->pages[idx]->addr;
    } else if (requested & VM_ACCESS_WRITE) {
        if (amap->pages[idx]) {
            assert(refcount_get(&amap->pages[idx]->count) > 0);

            /* If the reference count is greater than 1 we must copy it. Shared
             * regions should not contain any pages with a reference count
             * greater than 1. */
            if (refcount_get(&amap->pages[idx]->count) > 1) {
                assert(region->flags & VM_MAP_PRIVATE);

                dprintf(
                    "vm:  anon write fault: copying page %zu (addr: 0x%" PRIxPHYS ", refcount: %" PRId32 ")\n",
                    idx, amap->pages[idx]->addr, amap->pages[idx]->count);

                page = page_copy(amap->pages[idx], MM_KERNEL);
                refcount_inc(&page->count);

                /* Decrease the count of the old page. We must handle it going
                 * to 0 here, as another object could have released the page
                 * while we were copying. */
                if (refcount_dec(&amap->pages[idx]->count) == 0)
                    page_free(amap->pages[idx]);

                amap->pages[idx] = page;
            }

            phys = amap->pages[idx]->addr;
        } else {
            assert(region->flags & VM_MAP_PRIVATE);
            assert(region->handle);

            /* Find the page to copy. If there was an existing mapping, we
             * already have its address in phys so we don't need to bother
             * getting a page from the object again. */
            page_t *prev;
            if (!exist) {
                assert(region->ops && region->ops->get_page);

                ret = region->ops->get_page(region, offset + region->obj_offset, &prev);
                if (ret != STATUS_SUCCESS) {
                    dprintf(
                        "vm: failed to get page at offset 0x%" PRIx64 " from %p: %d\n",
                        offset + region->obj_offset, region->handle, ret);

                    mutex_unlock(&amap->lock);
                    return ret;
                }

                phys = prev->addr;
            } else {
                /* We do need to lookup the existing page in order to release
                 * it, however. Page may not necessarily exist here if something
                 * has a private mapping over device memory. */
                prev = page_lookup(phys);
            }

            dprintf(
                "vm:  anon write fault: copying page 0x%" PRIxPHYS " from %p\n",
                phys, region->handle);

            page = page_alloc(MM_KERNEL);
            phys_copy(page->addr, phys, MM_KERNEL);

            /* Add the page and release the old one. */
            refcount_inc(&page->count);
            amap->pages[idx] = page;
            if (prev && region->ops && region->ops->release_page)
                region->ops->release_page(region, prev);

            amap->curr_size++;
            phys = page->addr;
        }
    } else {
        if (amap->pages[idx]) {
            assert(refcount_get(&amap->pages[idx]->count) > 0);

            /* If the reference count is greater than 1, map read only so we
             * copy it if there is a later write to the page. */
            if (refcount_get(&amap->pages[idx]->count) > 1) {
                assert(region->flags & VM_MAP_PRIVATE);
                access &= ~VM_ACCESS_WRITE;
            }

            phys = amap->pages[idx]->addr;
        } else {
            assert(region->flags & VM_MAP_PRIVATE);
            assert(region->handle);
            assert(region->ops && region->ops->get_page);

            /* Get the page from the source, and map read-only. */
            ret = region->ops->get_page(region, offset + region->obj_offset, &page);
            if (ret != STATUS_SUCCESS) {
                dprintf(
                    "vm: failed to get page at offset 0x%" PRIx64 " from %p: %d\n",
                    offset + region->obj_offset, region->handle, ret);

                mutex_unlock(&amap->lock);
                return ret;
            }

            dprintf(
                "vm:  anon read fault: mapping page 0x%" PRIxPHYS " from %p as read-only\n",
                phys, region->handle);

            phys = page->addr;
            access &= ~VM_ACCESS_WRITE;
        }
    }

    /* The page address should now be stored in phys, and access flags should be
     * set correctly. If there is an existing mapping, remove it. */
    if (exist) {
        if (!mmu_context_unmap(region->as->mmu, addr, true, NULL))
            fatal("Could not remove previous mapping for %p", addr);
    }

    /* Map the entry in. Should always succeed with MM_KERNEL set. */
    mmu_context_map(region->as->mmu, addr, phys, access, MM_KERNEL);

    dprintf(
        "vm: mapped 0x%" PRIxPHYS " at %p (as: %p, access: 0x%x)\n",
        phys, addr, region->as, access);

    mutex_unlock(&amap->lock);

    if (_phys)
        *_phys = phys;

    return STATUS_SUCCESS;
}

/** Map a page from an object into an address space.
 * @note                Address space and MMU context should be locked. */
static status_t map_object_page(vm_region_t *region, ptr_t addr, phys_ptr_t *_phys) {
    status_t ret;

    assert(region->handle);

    /* Check if the page is already mapped. */
    phys_ptr_t phys;
    uint32_t flags;
    if (mmu_context_query(region->as->mmu, addr, &phys, &flags)) {
        if (_phys)
            *_phys = phys;

        return STATUS_SUCCESS;
    }

    assert(region->ops);
    assert(region->ops->get_page);

    /* Get a page from the object. */
    offset_t offset = (offset_t)(addr - region->start) + region->obj_offset;
    page_t *page;
    ret = region->ops->get_page(region, offset, &page);
    if (ret != STATUS_SUCCESS) {
        dprintf(
            "vm: failed to get page at offset 0x%" PRIx64 " from %p: %d\n",
            offset, region->handle, ret);
        return ret;
    }

    /* Map the entry in. FIXME: Once page reservations are implemented we should
     * reserve pages right at the beginning of the fault handler before locking
     * the address space, as if pages need to be reclaimed we could run into
     * issues because we're holding the address space and context locks. */
    mmu_context_map(region->as->mmu, addr, page->addr, region->access, MM_KERNEL);

    dprintf(
        "vm: mapped 0x%" PRIxPHYS " at %p (as: %p, access: 0x%x)\n",
        page->addr, addr, region->as, region->access);

    if (_phys)
        *_phys = page->addr;

    return STATUS_SUCCESS;
}

/** Map a page for a region into its address space.
 * @note                Address space and MMU context should be locked. */
static status_t map_page(vm_region_t *region, ptr_t addr, uint32_t requested, phys_ptr_t *_phys) {
    assert(vm_region_contains(region, addr));

    return (region->amap)
        ? map_anon_page(region, addr, requested, _phys)
        : map_object_page(region, addr, _phys);
}

/** Unmap a page within a region.
 * @note                Address space and MMU context should be locked. */
static bool unmap_page(vm_region_t *region, ptr_t addr) {
    assert(vm_region_contains(region, addr));

    offset_t offset = addr - region->start;

    page_t *page;
    if (!mmu_context_unmap(region->as->mmu, addr, true, &page))
        return false;

    /* Release the page from the source. */
    if (region->amap) {
        offset += region->amap_offset;
        size_t idx = offset >> PAGE_WIDTH;

        assert(idx < region->amap->max_size);

        /* If page is in the object, then do nothing. */
        if (region->amap->pages[idx]) {
            assert(region->amap->pages[idx] == page);
            return true;
        }

        assert(region->handle);
    }

    if (page && region->ops && region->ops->release_page) {
        offset += region->obj_offset;
        region->ops->release_page(region, page);
    }

    return true;
}


/**
 * Region functions.
 */

/** Clone a region.
 * @param src           Region to clone.
 * @param as            Address space for new region.
 * @return              Pointer to cloned region. */
static vm_region_t *vm_region_clone(vm_region_t *src, vm_aspace_t *as) {
    vm_region_t *dest = slab_cache_alloc(vm_region_cache, MM_KERNEL);

    dest->name    = (src->name) ? kstrdup(src->name, MM_KERNEL) : NULL;
    dest->as      = as;
    dest->start   = src->start;
    dest->size    = src->size;
    dest->access  = src->access;
    dest->flags   = src->flags;
    dest->state   = src->state;
    dest->ops     = src->ops;
    dest->private = src->private;

    if (src->state != VM_REGION_ALLOCATED) {
        dest->handle      = NULL;
        dest->obj_offset  = 0;
        dest->amap        = NULL;
        dest->amap_offset = 0;

        return dest;
    }

    /* Copy the object handle. */
    dest->handle     = src->handle;
    dest->obj_offset = src->obj_offset;
    if (dest->handle)
        object_handle_retain(dest->handle);

    if (src->flags & VM_MAP_PRIVATE) {
        /* This is a private region. Write-protect all mappings on the source
         * region and then clone the anonymous map. */
        mmu_context_lock(src->as->mmu);
        mmu_context_remap(src->as->mmu, src->start, src->size, src->access & ~VM_ACCESS_WRITE);
        mmu_context_unlock(src->as->mmu);

        assert(src->amap);

        dest->amap        = vm_amap_clone(src->amap, src->amap_offset, src->size);
        dest->amap_offset = 0;

        dprintf(
            "vm: copied private region %p (map: %p) to %p (map: %p)\n",
            src, src->amap, dest, dest->amap);
    } else {
        /* This is not a private mapping, just point the new region at the old
         * anonymous map. */
        dest->amap        = src->amap;
        dest->amap_offset = src->amap_offset;

        if (dest->amap) {
            refcount_inc(&dest->amap->count);
            vm_amap_map(dest->amap, dest->amap_offset, dest->size);
        }
    }

    return dest;
}

/** Searches for a region containing an address.
 * @param as            Address space to search in (should be locked).
 * @param addr          Address to search for.
 * @param unused        Whether to include unused regions in the search.
 * @return              Pointer to region if found, false if not. If including
 *                      unused regions, this will always succeed unless the
 *                      given address is invalid. */
static vm_region_t *vm_region_find(vm_aspace_t *as, ptr_t addr, bool unused) {
    /* Check if the cached pointer matches. Caching the last found region helps
     * mainly for page fault handling when code is hitting different parts of a
     * newly-mapped region in succession. */
    if (as->find_cache && vm_region_contains(as->find_cache, addr))
        return as->find_cache;

    vm_region_t *region, *near = NULL;

    /* Fall back on searching through the AVL tree. */
    avl_tree_node_t *node = as->tree.root;
    while (node) {
        region = avl_tree_entry(node, vm_region_t, tree_link);
        assert(region->state == VM_REGION_ALLOCATED);
        if (addr >= region->start) {
            if (vm_region_contains(region, addr)) {
                as->find_cache = region;
                return region;
            }

            near = avl_tree_entry(node, vm_region_t, tree_link);
            node = node->right;
        } else {
            node = node->left;
        }
    }

    /* We couldn't find a matching allocated region. We do however have the
     * nearest allocated region on the left. If we want to include unused
     * regions in the search, search forward from this region in the region
     * list to find the required region. */
    if (unused) {
        if (near) {
            region = vm_region_next(near);
        } else {
            /* Should never be empty. */
            assert(!list_empty(&as->regions));
            region = list_first(&as->regions, vm_region_t, header);
        }

        while (region) {
            if (vm_region_contains(region, addr)) {
                assert(region->state != VM_REGION_ALLOCATED);
                return region;
            }

            region = vm_region_next(region);
        }
    }

    return NULL;
}

/**
 * Maps physical memory for a region. This can be used for doing up-front
 * mappings in implementations of map() functions for devices.
 *
 * @param region        Region to map for.
 * @param base          Base address of the object's physical memory. The
 *                      region's object offset will be added on to this.
 * @param size          Total size of the object's physical memory. If the
 *                      region goes out of bounds, only pages within bounds will
 *                      be mapped, and if the out of bounds region is accessed,
 *                      a fault will occur.
 * @param flags         MMU mapping flags. Access flags are ignored from here,
 *                      only cacheability flags are used. Access flags are taken
 *                      from the region.
 * @param mmflag        Allocation behaviour flags.
 *
 * @return              Status code describing the result of the operation.
 */
status_t vm_region_map(
    vm_region_t *region, phys_ptr_t base, phys_size_t size, uint32_t flags,
    unsigned mmflag)
{
    assert(!(base % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));
    assert(base + size >= base);

    phys_ptr_t end      = base + size;
    phys_ptr_t map_base = base + min(region->obj_offset, (offset_t)size);
    size_t map_size     = min(region->size, end - map_base);

    flags = (flags & MMU_CACHE_MASK) | region->access;

    status_t ret = STATUS_SUCCESS;

    if (map_size > 0) {
        mmu_context_lock(region->as->mmu);

        for (size_t offset = 0; offset < map_size; offset += PAGE_SIZE) {
            ret = mmu_context_map(
                region->as->mmu, region->start + offset, map_base + offset,
                flags, mmflag);
            if (ret != STATUS_SUCCESS)
                break;
        }

        mmu_context_unlock(region->as->mmu);
    }

    return ret;
}

/**
 * This function is called whenever part of a region is going to be removed. It
 * unmaps pages covering the area, and updates the region's anonymous map (if it
 * has one). Does not release the anonymous map and object if the entire region
 * is being removed - this is done in vm_region_destroy() since only that
 * function should be used to remove an entire region.
 *
 * @param region        Region being unmapped (should not be reserved).
 * @param start         Start of range to unmap.
 * @param size          Size of range to unmap.
 */
static void vm_region_unmap(vm_region_t *region, ptr_t start, size_t size) {
    assert(region->state == VM_REGION_ALLOCATED);
    assert(region->handle || region->amap);

    /* Wait until the region becomes unlocked. TODO: we should keep track of
     * which parts of a region are locked and only wait if we're trying to unmap
     * over that part. */
    while (region->locked)
        condvar_wait(&region->waiters, &region->as->lock);

    mmu_context_lock(region->as->mmu);

    /* Unmap pages covering the region. */
    for (size_t i = 0; i < size; i += PAGE_SIZE)
        unmap_page(region, start + i);

    mmu_context_unlock(region->as->mmu);

    /* Release the pages in the anonymous map. */
    if (region->amap) {
        offset_t offset = (start - region->start) + region->amap_offset;
        vm_amap_unmap(region->amap, offset, size);
    }
}

/** Unmap an entire region. */
static void vm_region_destroy(vm_region_t *region) {
    /* Unmap the region and drop references to the object/anonymous map, and
     * remove it from the tree or freelist. */
    if (region->state == VM_REGION_ALLOCATED) {
        vm_region_unmap(region, region->start, region->size);

        if (region->amap)
            vm_amap_release(region->amap);

        if (region->handle)
            object_handle_release(region->handle);

        avl_tree_remove(&region->as->tree, &region->tree_link);
    } else if (region->state == VM_REGION_FREE) {
        vm_freelist_remove(region);
    }

    /* Remove from the main region list. */
    list_remove(&region->header);

    /* If the region was the cached find pointer, get rid of it. */
    if (region == region->as->find_cache)
        region->as->find_cache = NULL;

    assert(list_empty(&region->free_link));
    kfree(region->name);
    slab_cache_free(vm_region_cache, region);
}

/**
 * Kernel internal API functions.
 */

/**
 * Locks a single page into an address space for access with the specified
 * access. While the page is locked, it will not be evicted from the address
 * space, and it is guaranteed to be safe to access when the address space is
 * active. Note that this function places a lock on the whole region, preventing
 * it from being unmapped. Any thread that attempts to do so will block until
 * it is unlocked. Therefore, locks placed using this function should be short
 * lived.
 *
 * @param as            Address space to lock in.
 * @param addr          Address of page to lock.
 * @param access        Required access to the page.
 * @param _phys         Where to store physical address of page.
 *
 * @return              Status code describing the result of the operation.
 */
status_t vm_lock_page(vm_aspace_t *as, ptr_t addr, uint32_t access, phys_ptr_t *_phys) {
    assert(!(addr % PAGE_SIZE));

    mutex_lock(&as->lock);

    vm_region_t *region = vm_region_find(as, addr, false);
    if (!region || region->state != VM_REGION_ALLOCATED) {
        mutex_unlock(&as->lock);
        return STATUS_INVALID_ADDR;
    }

    /* Check whether the access is allowed. */
    if ((region->access & access) != access) {
        mutex_unlock(&as->lock);
        return STATUS_ACCESS_DENIED;
    }

    /* Don't allow locking the guard page of a stack. TODO: Stack direction. */
    if (region->flags & VM_MAP_STACK && addr == region->start) {
        mutex_unlock(&as->lock);
        return STATUS_INVALID_ADDR;
    }

    /* For now we just ensure that the page is mapped for the requested access,
     * as we don't evict pages at all. */
    mmu_context_lock(as->mmu);
    status_t ret = map_page(region, addr, access, _phys);
    mmu_context_unlock(as->mmu);

    /* Increase the locking count. */
    if (ret == STATUS_SUCCESS)
        region->locked++;

    mutex_unlock(&as->lock);
    return ret;
}

/** Unlocks a previously locked page.
 * @param as            Address space to unlock in.
 * @param addr          Address of page to unlock. */
void vm_unlock_page(vm_aspace_t *as, ptr_t addr) {
    mutex_lock(&as->lock);

    /* This should only be done after a call to vm_lock_page() so the region
     * should exist. */
    vm_region_t *region = vm_region_find(as, addr, false);
    if (!region || region->state != VM_REGION_ALLOCATED)
        fatal("Invalid call to vm_unlock_page(%p)", addr);

    /* Unblock any threads waiting for the region to be unlocked. */
    assert(region->locked);
    if (--region->locked == 0)
        condvar_broadcast(&region->waiters);

    mutex_unlock(&as->lock);
}

/**
 * Page fault handler.
 */

/** Page fault handler.
 * @param addr          Address the fault occurred at.
 * @param reason        Reason for the fault.
 * @param access        Type of memory access that caused the fault.
 * @return              Whether the fault was handled. */
bool vm_fault(frame_t *frame, ptr_t addr, int reason, uint32_t access) {
    /* If we're in interrupt context, this is certainly a kernel exception
     * that we cannot handle. Just bail out now so that we crash with details
     * of the exception rather than hitting the !in_interrupt() assert in
     * mutex_lock(). */
    if (in_interrupt())
        return false;

    assert(!local_irq_state());

    exception_info_t exception;
    memset(&exception, 0, sizeof(exception));
    exception.addr = (void *)addr;

    bool user = frame_from_user(frame);

    vm_aspace_t *as = curr_cpu->aspace;

    dprintf(
        "vm: %s mode page fault at %p (thread: %" PRId32 ", as: %p, reason: %d, access: 0x%" PRIx32 ")\n",
        (user) ? "user" : "kernel", addr, curr_thread->id, as, reason, access);

    /* If we don't have an address space, don't do anything. There won't be
     * anything to send a signal to, either. */
    if (unlikely(!as))
        return false;

    /* Safe to take the lock despite us being in an interrupt - the lock is only
     * only held within the functions in this file, and they should not incur a
     * page fault (if they do there's something wrong!). */
    if (unlikely(mutex_held(&as->lock) && as->lock.holder == curr_thread)) {
        kprintf(LOG_WARN, "vm: fault on %p with lock held at %pS\n", as, frame->ip);
        return false;
    }

    bool in_usermem = curr_thread->in_usermem;
    curr_thread->in_usermem = false;

    mutex_lock(&as->lock);

    /* Round down address to a page boundary. */
    ptr_t base = addr & PAGE_MASK;

    /* Find the region that the fault occurred in. */
    vm_region_t *region = vm_region_find(as, base, false);
    if (unlikely(!region)) {
        kprintf(
            LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p: no region found\n",
            curr_thread->id, curr_thread->name, addr);

        exception.code = EXCEPTION_ADDR_UNMAPPED;
        goto out;
    }

    assert(region->state == VM_REGION_ALLOCATED);
    assert(region->amap || region->handle);

    /* Check whether the access is allowed. */
    if (!(region->access & access)) {
        kprintf(
            LOG_NOTICE,
            "vm: thread %" PRId32 " (%s) page fault at %p: access violation (access: 0x%x, allowed: 0x%x)\n",
            curr_thread->id, curr_thread->name, addr, access, region->access);

        exception.code = EXCEPTION_ACCESS_VIOLATION;
        exception.access = access;
        goto out;
    }

    /* If the region is a stack region, check if we've hit the guard page.
     * TODO: Stack direction. */
    if (region->flags & VM_MAP_STACK && base == region->start) {
        kprintf(
            LOG_NOTICE,
            "vm: thread %" PRId32 " (%s) page fault at %p: hit stack guard page\n",
            curr_thread->id, curr_thread->name, addr);

        exception.code = EXCEPTION_STACK_OVERFLOW;
        goto out;
    }

    mmu_context_lock(as->mmu);
    local_irq_enable();

    if (region->amap) {
        /* Do some sanity checks if this is an access violation fault. The only
         * access faults we should get are write faults. COW faults should never
         * occur on non-private regions, either. */
        if (reason == VM_FAULT_ACCESS) {
            if (access != VM_ACCESS_WRITE) {
                fatal("Non-write access fault at %p on %p (%d)", addr, region->amap, access);
            } else if (!(region->flags & VM_MAP_PRIVATE)) {
                fatal("Copy-on-write fault at %p on non-private region", addr);
            }
        }

        exception.status = map_anon_page(region, base, access, NULL);
    } else {
        exception.status = map_object_page(region, base, NULL);
    }

    local_irq_disable();
    mmu_context_unlock(as->mmu);

    if (exception.status != STATUS_SUCCESS) {
        exception.code = EXCEPTION_PAGE_ERROR;

        kprintf(
            LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p: failed to map page: %d\n",
            curr_thread->id, curr_thread->name, addr, exception.status);
    }

out:
    mutex_unlock(&as->lock);
    curr_thread->in_usermem = in_usermem;

    if (unlikely(exception.code)) {
        if (user) {
            thread_exception(&exception);
        } else if (curr_thread->in_usermem && is_user_address((void *)addr)) {
            /* Handle faults in safe user memory access functions. */
            kprintf(
                LOG_DEBUG,
                "vm: thread %" PRId32 " (%s) faulted in user memory access at %p (ip: %p)\n",
                curr_thread->id, curr_thread->name, addr, frame->ip);

            longjmp(curr_thread->usermem_context, 1);
        } else {
            return false;
        }
    }

    return true;
}

/**
 * Public API implementation.
 */

/** Cut out the specified space from the address space.
 * @param as            Address space to trim.
 * @param start         Start address of the region to trim.
 * @param size          Size of region to trim.
 * @return              Pointer to region preceding the trimmed area, NULL if
 *                      there is no preceding region. */
static vm_region_t *trim_regions(vm_aspace_t *as, ptr_t start, size_t size) {
    ptr_t end = start + size - 1;

    /* Find the region containing the start address. */
    vm_region_t *next = vm_region_find(as, start, true);
    assert(next);

    /* Save the region to return to the caller. If the start address matches
     * the start of the area to trim we need to return the region before it. */
    vm_region_t *prev = (next->start == start) ? vm_region_prev(next) : next;

    while (next) {
        vm_region_t *region = next;
        next = vm_region_next(region);

        /* Calculate the overlapping part of the region. If there is none, we
         * can finish. */
        ptr_t region_end  = region->start + region->size - 1;
        ptr_t match_start = max(start, region->start);
        ptr_t match_end   = min(end, region_end);
        if (match_end <= match_start)
            break;

        /* If we match the whole region, destroy it and move on. */
        if (match_start == region->start && match_end == region_end) {
            vm_region_destroy(region);
            continue;
        }

        /* Unmap pages in the area we're not going to cover any more. */
        if (region->state == VM_REGION_ALLOCATED)
            vm_region_unmap(region, match_start, match_end - match_start + 1);

        ptr_t new_start;
        size_t new_size;

        if (match_start == region->start) {
            /* Cutting out from the start of the region. */
            new_start = match_end + 1;
            new_size  = region_end - match_end;
        } else if (match_end == region_end) {
            /* Cutting out from the end of the region. */
            new_start = region->start;
            new_size  = match_start - region->start;
        } else {
            /* Cutting out from the middle of the region. In this case we must
             * split the region. Existing region becomes the bottom half of the
             * split, we create a new one for the top half. */
            new_start = region->start;
            new_size  = match_start - region->start;

            vm_region_t *split = slab_cache_alloc(vm_region_cache, MM_KERNEL);

            split->name        = (region->name) ? kstrdup(region->name, MM_KERNEL) : NULL;
            split->as          = as;
            split->start       = match_end + 1;
            split->size        = region_end - match_end;
            split->access      = region->access;
            split->flags       = region->flags;
            split->state       = region->state;
            split->ops         = region->ops;
            split->private     = region->private;
            split->handle      = region->handle;
            split->obj_offset  = region->obj_offset;
            split->amap        = region->amap;
            split->amap_offset = region->amap_offset;

            if (split->state == VM_REGION_ALLOCATED) {
                if (split->handle)
                    object_handle_retain(split->handle);

                if (split->amap) {
                    refcount_inc(&split->amap->count);
                    split->amap_offset += split->start - region->start;
                } else {
                    split->obj_offset += split->start - region->start;
                }

                /* Insert the split region to the tree. */
                avl_tree_insert(&as->tree, split->start, &split->tree_link);
            } else if (split->state == VM_REGION_FREE) {
                /* Insert the split region to the free list. */
                vm_freelist_insert(split, split->size);
            }

            /* Put the split after the region in the list, then on next
             * iteration we can break because we won't overlap any of the next
             * region. */
            list_add_after(&region->header, &split->header);
            next = NULL;
        }

        if (new_start != region->start && region->state == VM_REGION_ALLOCATED) {
            /* Reinsert into the tree with the new start. */
            avl_tree_remove(&as->tree, &region->tree_link);
            avl_tree_insert(&as->tree, new_start, &region->tree_link);

            /* Increase the object offsets. */
            if (region->amap) {
                region->amap_offset += new_start - region->start;
            } else {
                region->obj_offset += new_start - region->start;
            }
        }

        if (new_size != region->size && region->state == VM_REGION_FREE) {
            /* Size changed, move to the correct free list. */
            vm_freelist_remove(region);
            vm_freelist_insert(region, new_size);
        }

        region->start = new_start;
        region->size  = new_size;
    }

    return prev;
}

/** Insert a region, replacing overlapping existing regions.
 * @param as            Address space to insert into.
 * @param region        Region to insert. Start address and size may be modified
 *                      if inserting a free or reserved region due to coalescing.
 *                      This region will have been inserted into the tree or
 *                      free lists as necessary. */
static void insert_region(vm_aspace_t *as, vm_region_t *region) {
    /* Create a hole to insert the new region into. */
    vm_region_t *exist = trim_regions(as, region->start, region->size);
    if (exist) {
        assert((exist->start + exist->size) == region->start);

        list_add_after(&exist->header, &region->header);

        /* Merge adjacent unused regions. */
        if (vm_region_mergeable(region, exist)) {
            region->start = exist->start;
            region->size += exist->size;
            vm_region_destroy(exist);
        }
    } else {
        list_prepend(&as->regions, &region->header);
    }

    /* Check if we can merge with the region after. */
    exist = vm_region_next(region);
    if (exist) {
        assert(exist->start == (region->start + region->size));

        if (vm_region_mergeable(region, exist)) {
            region->size += exist->size;
            vm_region_destroy(exist);
        }
    }

    /* Finally, insert into the region tree or the free lists. */
    if (region->state == VM_REGION_ALLOCATED) {
        avl_tree_insert(&as->tree, region->start, &region->tree_link);
    } else if (region->state == VM_REGION_FREE) {
        vm_freelist_insert(region, region->size);
    }
}

static vm_region_t *create_free_region(vm_aspace_t *as, ptr_t start, size_t size, int state) {
    vm_region_t *region = slab_cache_alloc(vm_region_cache, MM_KERNEL);

    region->as          = as;
    region->start       = start;
    region->size        = size;
    region->access      = 0;
    region->flags       = 0;
    region->state       = state;
    region->handle      = NULL;
    region->obj_offset  = 0;
    region->amap        = NULL;
    region->amap_offset = 0;
    region->ops         = NULL;
    region->private     = NULL;
    region->name        = NULL;

    return region;
}

/** Allocate space in an address space.
 * @param as            Address space to allocate in (should be locked).
 * @param base          Base to start searching from.
 * @param size          Size of space required.
 * @param align         Alignment of space required.
 * @param access        Access flags for the region.
 * @param flags         Flags for the region.
 * @param name          Name of the region (will not be copied).
 * @return              Pointer to region if allocated, NULL if not. */
static vm_region_t *alloc_region(
    vm_aspace_t *as, ptr_t base, size_t size, size_t align, uint32_t access,
    uint32_t flags, char *name)
{
    /* Get the list to search on. If the size is exactly a power of 2, then
     * regions on freelist[n] are guaranteed to be big enough. Otherwise, use
     * freelist[n + 1] so that we ensure that all regions we find are large
     * enough. However, only do this if there are available regions in higher
     * lists. */
    unsigned list = highbit(size) - PAGE_WIDTH - 1;
    if ((size & (size - 1)) != 0 && as->free_map >> (list + 1))
        list++;

    /* Find a free region. */
    vm_region_t *candidate = NULL;
    ptr_t candidate_start;
    for (unsigned i = list; i < VM_FREELISTS; i++) {
        list_foreach(&as->free[i], iter) {
            vm_region_t *region = list_entry(iter, vm_region_t, free_link);

            assert(region->state == VM_REGION_FREE);

            /* See if this region can satisfy the alignment constraints. */
            ptr_t align_start = round_up_pow2(region->start, align);
            size_t offset     = align_start - region->start;

            if (offset > region->size || region->size - offset < size)
                continue;

            /* See if this region is above the requested search base. */
            ptr_t base_start = max(align_start, base);
            offset = base_start - region->start;

            if (offset > region->size || region->size - offset < size) {
                /* Save as a candidate for if we can't find a region above the
                 * base. */
                if (!candidate) {
                    candidate       = region;
                    candidate_start = align_start;
                }

                continue;
            }

            /* This region is acceptable immediately. */
            candidate       = region;
            candidate_start = base_start;

            i = VM_FREELISTS;
            break;
        }
    }

    if (!candidate)
        return NULL;

    vm_freelist_remove(candidate);

    /* If the start point is within the region, split it. */
    if (candidate_start != candidate->start) {
        vm_region_t *split = create_free_region(
            as, candidate->start, candidate_start - candidate->start,
            VM_REGION_FREE);

        vm_freelist_insert(split, split->size);
        list_add_before(&candidate->header, &split->header);

        candidate->start = candidate_start;
        candidate->size -= split->size;
    }

    /* If the region is still too big, split it. */
    if (candidate->size > size) {
        vm_region_t *split = create_free_region(
            as, candidate->start + size, candidate->size - size,
            VM_REGION_FREE);

        vm_freelist_insert(split, split->size);
        list_add_after(&candidate->header, &split->header);

        candidate->size = size;
    }

    /* Set region state and add to the tree. */
    candidate->access = access;
    candidate->flags  = flags;
    candidate->state  = VM_REGION_ALLOCATED;
    candidate->name   = name;

    avl_tree_insert(&as->tree, candidate->start, &candidate->tree_link);

    dprintf(
        "vm: allocated region [%p,%p) in %p\n",
        candidate->start, candidate->start + candidate->size, as);

    return candidate;
}

/**
 * Creates a new memory mapping that maps either an object or anonymous memory.
 * The spec argument controls where the mapping will be placed. The following
 * address specifications are currently defined:
 *
 *  - VM_ADDRESS_ANY: The mapping can be placed anywhere available in the
 *    address space, an unused space will be allocated to fit it in.
 *  - VM_ADDRESS_EXACT: The mapping will be placed at exactly the address
 *    specified, and any existing mappings in the same region will be replaced.
 *  - VM_ADDRESS_HINT: Try to allocate unused space near to the specified
 *    address.
 *
 * The flags argument controls the behaviour of the mapping. The following flags
 * are currently defined:
 *
 *  - VM_MAP_PRIVATE: Modifications to the mapping will not be transferred
 *    through to the source object, and if the address space is duplicated, the
 *    duplicate and original will be given copy-on-write copies of the region.
 *    If this flag is not specified and the address space is duplicated, changes
 *    made in either address space will be visible in the other.
 *  - VM_MAP_OVERCOMMIT: Memory will not be reserved for the mapping, meaning
 *    it can be made larger than the total memory available (memory is only
 *    allocated when it is actually accessed). The default behaviour is to only
 *    allow mappings if the memory requirement can be satisfied.
 *
 * When mapping an object, the calling process must have the correct access
 * rights to the object for the mapping permissions requested.
 *
 * @param as            Address space to map in.
 * @param _addr         For VM_ADDRESS_ANY, points to a variable in which to
 *                      store the allocated address. For VM_ADDRESS_EXACT,
 *                      points to a variable containing the address to place
 *                      the mapping at.
 * @param size          Size of mapping (multiple of page size).
 * @param align         Alignment of the mapping (power-of-2 greater than or
 *                      equal to the page size, or 0 for any alignment). Ignored
 *                      for VM_ADDRESS_EXACT.
 * @param spec          Address specification (VM_ADDRESS_*).
 * @param access        Allowed access flags (VM_ACCESS_*).
 * @param flags         Mapping behaviour flags (VM_MAP_*).
 * @param handle        Handle to object to map in. If NULL, then the region
 *                      will be an anonymous memory mapping.
 * @param offset        Offset into object to map from (multiple of page size).
 * @param name          Name of the memory mapping, for informational purposes.
 *                      Can be NULL.
 *
 * @return              Status code describing result of the operation.
 */
status_t vm_map(
    vm_aspace_t *as, ptr_t *_addr, size_t size, size_t align, unsigned spec,
    uint32_t access, uint32_t flags, object_handle_t *handle, offset_t offset,
    const char *name)
{
    status_t ret;

    assert(_addr);

    if (!size || size % PAGE_SIZE) {
        return STATUS_INVALID_ARG;
    } else if (!access) {
        return STATUS_INVALID_ARG;
    }

    /* Check that the address specification is valid. */
    switch (spec) {
        case VM_ADDRESS_ANY:
        case VM_ADDRESS_EXACT:
        case VM_ADDRESS_HINT:
            break;
        default:
            return STATUS_INVALID_ARG;
    }

    /* Get address argument if necessary. */
    ptr_t addr;
    if (spec == VM_ADDRESS_EXACT || spec == VM_ADDRESS_HINT) {
        addr = *_addr;

        if (addr % PAGE_SIZE || addr + size < addr)
            return STATUS_INVALID_ARG;
    }

    /* Check alignment. */
    if (spec != VM_ADDRESS_EXACT) {
        if (!align) {
            align = PAGE_SIZE;
        } else if (align < PAGE_SIZE || !is_pow2(align)) {
            return STATUS_INVALID_ARG;
        }
    }

    if (handle) {
        if (offset % PAGE_SIZE || (offset_t)(offset + size) < offset) {
            return STATUS_INVALID_ARG;
        } else if (!handle->type->map) {
            return STATUS_NOT_SUPPORTED;
        }
    }

    /* Get the name to use. Get from object type if no name supplied. */
    char *dup = NULL;
    if (name) {
        dup = kstrdup(name, MM_KERNEL);
    } else if (handle && handle->type->name) {
        dup = handle->type->name(handle);
    }

    /* Cannot have a guard page on a 1-page stack. */
    if (flags & VM_MAP_STACK && size == PAGE_SIZE)
        flags &= ~VM_MAP_STACK;

    mutex_lock(&as->lock);

    /* Create the region according to the address specification. */
    vm_region_t *region = NULL;
    switch (spec) {
        case VM_ADDRESS_ANY:
            addr = USER_ANY_BASE;
        case VM_ADDRESS_HINT:
            /* Allocate some space. */
            region = alloc_region(as, addr, size, align, access, flags, dup);
            if (!region) {
                mutex_unlock(&as->lock);
                kfree(dup);
                return STATUS_NO_MEMORY;
            }

            break;
        case VM_ADDRESS_EXACT:
            if (!vm_aspace_fits(as, addr, size)) {
                mutex_unlock(&as->lock);
                kfree(dup);
                return STATUS_NO_MEMORY;
            }

            region = slab_cache_alloc(vm_region_cache, MM_KERNEL);

            region->as     = as;
            region->start  = addr;
            region->size   = size;
            region->access = access;
            region->flags  = flags;
            region->state  = VM_REGION_ALLOCATED;
            region->name   = dup;

            insert_region(as, region);
            break;
    }

    /* Attach the object to the region. */
    if (handle) {
        region->handle = handle;
        object_handle_retain(region->handle);
        region->obj_offset = offset;

        ret = handle->type->map(handle, region);
        if (ret != STATUS_SUCCESS) {
            /* Free up the region again. */
            insert_region(as, create_free_region(as, region->start, region->size, VM_REGION_FREE));
            mutex_unlock(&as->lock);
            return ret;
        }
    } else {
        region->handle     = NULL;
        region->obj_offset = 0;
        region->ops        = NULL;
        region->private    = NULL;
    }

    /* For private or anonymous mappings we must create an anonymous map. */
    region->amap_offset = 0;
    if (!handle || flags & VM_MAP_PRIVATE) {
        region->amap = vm_amap_create(size);
        vm_amap_map(region->amap, 0, size);
    } else {
        region->amap = NULL;
    }

    dprintf(
        "vm: mapped region [%p,%p) in %p (spec: %u, access: 0x%" PRIx32 ", flags: 0x%"
            PRIx32 ", handle: %p, offset: 0x%" PRIx64 ")\n",
        region->start, region->start + region->size, as, spec, access,
        flags, handle, offset);

    *_addr = region->start;

    mutex_unlock(&as->lock);
    return STATUS_SUCCESS;
}

/**
 * Marks the specified address range as free in an address space and unmaps
 * anything that may be mapped there.
 *
 * @param as            Address space to unmap from.
 * @param start         Start of region to free.
 * @param size          Size of region to free.
 *
 * @return              Status code describing result of the operation.
 */
status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size) {
    if (!size || start % PAGE_SIZE || size % PAGE_SIZE)
        return STATUS_INVALID_ARG;

    mutex_lock(&as->lock);

    if (!vm_aspace_fits(as, start, size)) {
        mutex_unlock(&as->lock);
        return STATUS_NO_MEMORY;
    }

    insert_region(as, create_free_region(as, start, size, VM_REGION_FREE));

    dprintf("vm: unmapped region [%p,%p) in %p\n", start, start + size, as);
    mutex_unlock(&as->lock);
    return STATUS_SUCCESS;
}

/**
 * Marks a region of memory in an address space as reserved. Reserved regions
 * will never be allocated from for VM_ADDRESS_ANY mappings, but they can be
 * overwritten by VM_ADDRESS_EXACT or removed by using vm_unmap() on them.
 *
 * @param as            Address space to reserve in.
 * @param start         Start of region to reserve.
 * @param size          Size of region to reserve.
 *
 * @return              Status code describing result of the operation.
 */
status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size) {
    if (!size || start % PAGE_SIZE || size % PAGE_SIZE)
        return STATUS_INVALID_ARG;

    mutex_lock(&as->lock);

    if (!vm_aspace_fits(as, start, size)) {
        mutex_unlock(&as->lock);
        return STATUS_NO_MEMORY;
    }

    insert_region(as, create_free_region(as, start, size, VM_REGION_RESERVED));

    dprintf("vm: reserved region [%p,%p) in %p\n", start, start + size, as);
    mutex_unlock(&as->lock);
    return STATUS_SUCCESS;
}

/** Switch to another address space.
 * @param as            Address space to switch to. */
void vm_aspace_switch(vm_aspace_t *as) {
    bool irq_state = local_irq_disable();

    /* The kernel process does not have an address space. When switching to one
     * of its threads, it is not necessary to switch to the kernel MMU context,
     * as all mappings in the kernel context are visible in all address spaces.
     * Kernel threads should never touch the userspace portion of the address
     * space. */
    if (as && as != curr_cpu->aspace) {
        /* Decrease old address space's reference count, if there is one. */
        if (curr_cpu->aspace) {
            mmu_context_unload(curr_cpu->aspace->mmu);
            refcount_dec(&curr_cpu->aspace->count);
        }

        /* Switch to the new address space. */
        refcount_inc(&as->count);
        mmu_context_load(as->mmu);
        curr_cpu->aspace = as;
    }

    local_irq_restore(irq_state);
}

/** Create a new address space.
 * @return              Pointer to address space structure. */
vm_aspace_t *vm_aspace_create(void) {
    status_t ret;

    vm_aspace_t *as = slab_cache_alloc(vm_aspace_cache, MM_KERNEL);

    as->mmu        = mmu_context_create(MM_KERNEL);
    as->find_cache = NULL;
    as->free_map   = 0;

    /* Insert the initial free region. */
    vm_region_t *region = slab_cache_alloc(vm_region_cache, MM_KERNEL);

    region->as          = as;
    region->start       = USER_BASE;
    region->size        = USER_SIZE;
    region->access      = 0;
    region->flags       = 0;
    region->state       = VM_REGION_FREE;
    region->handle      = NULL;
    region->obj_offset  = 0;
    region->amap        = NULL;
    region->amap_offset = 0;
    region->ops         = NULL;
    region->private     = NULL;
    region->name        = NULL;

    list_append(&as->regions, &region->header);
    vm_freelist_insert(region, USER_SIZE);

    /* Mark the first page of the address space as reserved to catch NULL
     * pointer accesses. This should not fail. */
    ret = vm_reserve(as, 0x0, PAGE_SIZE);
    assert(ret == STATUS_SUCCESS);

    return as;
}

/**
 * Creates a clone of an existing address space. Non-private regions will be
 * shared among the two address spaces (modifications in one will affect both),
 * whereas private regions will be duplicated via copy-on-write.
 *
 * @param parent        Original address space.
 *
 * @return              Pointer to new address space.
 */
vm_aspace_t *vm_aspace_clone(vm_aspace_t *parent) {
    vm_aspace_t *as = slab_cache_alloc(vm_aspace_cache, MM_KERNEL);

    as->mmu        = mmu_context_create(MM_KERNEL);
    as->find_cache = NULL;
    as->free_map   = 0;

    mutex_lock(&parent->lock);

    /* Clone each region in the original address space. */
    list_foreach(&parent->regions, iter) {
        vm_region_t *parent_region = list_entry(iter, vm_region_t, header);

        vm_region_t *region = vm_region_clone(parent_region, as);
        list_append(&as->regions, &region->header);

        /* Insert into the region tree or the free lists. */
        if (region->state == VM_REGION_ALLOCATED) {
            avl_tree_insert(&as->tree, region->start, &region->tree_link);
        } else if (region->state == VM_REGION_FREE) {
            vm_freelist_insert(region, region->size);
        }
    }

    mutex_unlock(&parent->lock);
    return as;
}

/** Switch away from an address space to the kernel MMU context.
 * @param as            Address space being switched from. */
static status_t switch_to_kernel(void *_as) {
    vm_aspace_t *as = _as;

    /* We may have switched address space between the check below and receiving
     * the interrupt. Avoid an unnecessary switch in this case. */
    if (as == curr_cpu->aspace) {
        mmu_context_unload(as->mmu);
        refcount_dec(&as->count);

        mmu_context_load(&kernel_mmu_context);
        curr_cpu->aspace = NULL;
    }

    return STATUS_SUCCESS;
}

/**
 * Removes all memory mappings in an address space and frees it. This must
 * not be called if the address space is in use on any CPU. There should also
 * be no references to it in any processes, to ensure that nothing will attempt
 * to access it while it is being destroyed.
 *
 * @param as            Address space to destroy.
 */
void vm_aspace_destroy(vm_aspace_t *as) {
    assert(as);

    /* If the address space is in use, it must mean that a CPU has not switched
     * away from it because it is now running a kernel thread (see the comment
     * in vm_aspace_switch()). We need to go through and prod any CPUs that are
     * using it. */
    if (refcount_get(&as->count) > 0) {
        bool irq_state = local_irq_disable();

        list_foreach(&running_cpus, iter) {
            cpu_t *cpu = list_entry(iter, cpu_t, header);

            if (cpu->aspace == as)
                smp_call_single(cpu->id, switch_to_kernel, as, 0);
        }

        local_irq_restore(irq_state);

        /* The address space should no longer be in use. */
        assert(refcount_get(&as->count) == 0);
    }

    /* Unmap and destroy each region. */
    list_foreach_safe(&as->regions, iter)
        vm_region_destroy(list_entry(iter, vm_region_t, header));

    /* Destroy the MMU context. */
    mmu_context_destroy(as->mmu);

    assert(list_empty(&as->regions));
    assert(avl_tree_empty(&as->tree));

    slab_cache_free(vm_aspace_cache, as);
}

/** Show information about a region within an address space. */
static kdb_status_t kdb_cmd_region(int argc, char **argv, kdb_filter_t *filter) {
    uint64_t val;

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<process ID|addr>] <addr>\n\n", argv[0]);

        kdb_printf("Prints details about the region containing the given address in the address\n");
        kdb_printf("space specified. If only one argument is given, the current process' address\n");
        kdb_printf("space will be used.\n");
        return KDB_SUCCESS;
    }

    vm_aspace_t *as;
    if (argc == 3) {
        if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        if (val >= KERNEL_BASE) {
            as = (vm_aspace_t *)((ptr_t)val);
        } else {
            process_t *process = process_lookup_unsafe(val);
            if (!process) {
                kdb_printf("Invalid process ID\n");
                return KDB_FAILURE;
            }

            as = process->aspace;
        }
    } else if (argc == 2) {
        as = curr_proc->aspace;
    } else {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help\n", argv[0]);
        return KDB_FAILURE;
    }

    if (!as) {
        kdb_printf("Invalid address space\n");
        return KDB_FAILURE;
    }

    if (kdb_parse_expression(argv[argc - 1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    vm_region_t *region = vm_region_find(as, val, true);
    if (!region) {
        kdb_printf("Region not found\n");
        return KDB_FAILURE;
    }

    kdb_printf("Region %p (%s)\n", region, (region->name) ? region->name : "<unnamed>");
    kdb_printf("=================================================\n");
    kdb_printf("as:          %p\n", region->as);
    kdb_printf("start:       %p\n", region->start);
    kdb_printf("size:        0x%zx\n", region->size);
    kdb_printf(
        "access:      %c%c%c (0x%" PRIx32 ")\n",
        (region->access & VM_ACCESS_READ) ? 'R' : '-',
        (region->access & VM_ACCESS_WRITE) ? 'W' : '-',
        (region->access & VM_ACCESS_EXECUTE) ? 'X' : '-',
        region->access);
    kdb_printf("flags:       0x%" PRIx32 "\n", region->flags);

    switch (region->state) {
        case VM_REGION_FREE:
            kdb_printf("state:       %d (free)\n", region->state);
            break;
        case VM_REGION_ALLOCATED:
            kdb_printf("state:       %d (allocated)\n", region->state);
            break;
        case VM_REGION_RESERVED:
            kdb_printf("state:       %d (reserved)\n", region->state);
            break;
        default:
            kdb_printf("state:       %d (invalid)\n", region->state);
            break;
    }

    kdb_printf("handle:      %p\n", region->handle);
    kdb_printf("obj_offset:  0x%" PRIx64 "\n", region->obj_offset);
    kdb_printf("amap:        %p\n", region->amap);

    if (region->amap) {
        kdb_printf(" count:      %d\n", refcount_get(&region->amap->count));
        kdb_printf(
            " lock:       %d (%" PRId32 ")\n",
            atomic_load(&region->amap->lock.value),
            (region->amap->lock.holder) ? region->amap->lock.holder->id : -1);
        kdb_printf(" curr_size:  %zu\n", region->amap->curr_size);
        kdb_printf(" max_size:   %zu\n", region->amap->max_size);
    }

    kdb_printf("amap_offset: 0x%" PRIx64 "\n", region->amap_offset);

    return KDB_SUCCESS;
}

/** Display details of a region.
 * @param region    Region to display. */
static void dump_region(vm_region_t *region) {
    kdb_printf(
        "%-18p 0x%-12zx %c%c%c     0x%-3" PRIx32 " ",
        region->start, region->size,
        (region->access & VM_ACCESS_READ) ? 'R' : '-',
        (region->access & VM_ACCESS_WRITE) ? 'W' : '-',
        (region->access & VM_ACCESS_EXECUTE) ? 'X' : '-',
        region->flags);

    switch (region->state) {
        case VM_REGION_FREE:
            kdb_printf("Free  ");
            break;
        case VM_REGION_ALLOCATED:
            kdb_printf("Alloc ");
            break;
        case VM_REGION_RESERVED:
            kdb_printf("Rsvd  ");
            break;
        default:
            kdb_printf("????? ");
            break;
    }

    kdb_printf(
        "0x%-8" PRIx64 " %s\n",
        region->obj_offset, (region->name) ? region->name : "<unnamed>");
}

/** Modes for address space dumping. */
enum {
    DUMP_ALL,
    DUMP_ALLOCATED,
    DUMP_FREE,
};

/** Dump an address space. */
static kdb_status_t kdb_cmd_aspace(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<process ID|addr>]\n", argv[0]);
        kdb_printf("       %s --allocated [<process ID|addr>]\n", argv[0]);
        kdb_printf("       %s --free [<process ID|addr>]\n\n", argv[0]);

        kdb_printf("The first form prints some details about an address space and a list of all\n");
        kdb_printf("regions (free, reserved and allocated) from the sorted region list. The second\n");
        kdb_printf("form prints the content of the allocated region tree. The final form prints the\n");
        kdb_printf("content of the address space's free lists.\n\n");

        kdb_printf("If no address space is specified, the current process' address space will be\n");
        kdb_printf("used.\n");
        return KDB_SUCCESS;
    }

    int pos  = 1;
    int mode = DUMP_ALL;

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
        if (strcmp(argv[1], "--free") == 0) {
            mode = DUMP_FREE;
        } else if (strcmp(argv[1], "--allocated") == 0) {
            mode = DUMP_ALLOCATED;
        } else {
            kdb_printf("Unrecognized option. See 'help %s' for help\n", argv[0]);
            return KDB_FAILURE;
        }

        pos++;
    }

    if (pos != argc && pos != argc - 1) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help\n", argv[0]);
        return KDB_FAILURE;
    }

    process_t *process;
    vm_aspace_t *as;

    if (pos != argc) {
        uint64_t val;
        if (kdb_parse_expression(argv[pos], &val, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        if (val >= KERNEL_BASE) {
            process = NULL;
            as = (vm_aspace_t *)((ptr_t)val);
        } else {
            process = process_lookup_unsafe(val);
            if (!process) {
                kdb_printf("Invalid process ID\n");
                return KDB_FAILURE;
            }

            as = process->aspace;
        }
    } else {
        process = curr_proc;
        as = process->aspace;
    }

    if (!as) {
        kdb_printf("Invalid address space\n");
        return KDB_FAILURE;
    }

    if (mode == DUMP_ALL) {
        if (process) {
            kdb_printf("Aspace %p (%s)\n", as, process->name);
        } else {
            kdb_printf("Aspace %p\n", as);
        }

        kdb_printf("=================================================\n");

        kdb_printf(
            "lock:       %d (%" PRId32 ")\n",
            atomic_load(&as->lock.value), (as->lock.holder) ? as->lock.holder->id : -1);
        kdb_printf("count:      %d\n", refcount_get(&as->count));
        kdb_printf("find_cache: %p\n", as->find_cache);
        kdb_printf("mmu:        %p\n", as->mmu);
        kdb_printf("free_map:   0x%lx\n\n", as->free_map);
    }

    if (mode == DUMP_FREE)
        kdb_printf("List ");

    kdb_printf(
        "%-18s %-14s %-7s %-5s %-5s %-10s %s\n",
        "Start", "Size", "Access", "Flags", "State", "Offset", "Name");

    if (mode == DUMP_FREE)
        kdb_printf("==== ");

    kdb_printf(
        "%-18s %-14s %-7s %-5s %-5s %-10s %s\n",
        "=====", "====", "======", "=====", "=====", "======", "====");

    switch (mode) {
        case DUMP_ALL:
            list_foreach(&as->regions, iter)
                dump_region(list_entry(iter, vm_region_t, header));

            break;
        case DUMP_ALLOCATED:
            avl_tree_foreach(&as->tree, iter)
                dump_region(avl_tree_entry(iter, vm_region_t, tree_link));

            break;
        case DUMP_FREE:
            for (unsigned i = 0; i < VM_FREELISTS; i++) {
                list_foreach(&as->free[i], iter) {
                    kdb_printf("%-4u ", i);
                    dump_region(list_entry(iter, vm_region_t, free_link));
                }
            }

            break;
    }

    return KDB_SUCCESS;
}

/** Initialize the VM system. */
__init_text void vm_init(void) {
    /* Create the VM slab caches. */
    vm_aspace_cache = object_cache_create(
        "vm_aspace_cache",
        vm_aspace_t, vm_aspace_ctor, NULL, NULL, 0, MM_BOOT);
    vm_region_cache = object_cache_create(
        "vm_region_cache",
        vm_region_t, vm_region_ctor, NULL, NULL, 0, MM_BOOT);
    vm_amap_cache = object_cache_create(
        "vm_amap_cache",
        vm_amap_t, vm_amap_ctor, NULL, NULL, 0, MM_BOOT);

    /* Bring up the page daemons. */
    page_daemon_init();

    /* Initialize the caching system. */
    vm_cache_init();

    /* Register the KDB commands. */
    kdb_register_command("region", "Print details about a VM region.", kdb_cmd_region);
    kdb_register_command("aspace", "Print details about an address space.", kdb_cmd_aspace);
}

/**
 * User API.
 */

/**
 * Creates a new memory mapping that maps either an object or anonymous memory.
 * The spec argument controls where the mapping will be placed. The following
 * address specifications are currently defined:
 *
 *  - VM_ADDRESS_ANY: The mapping can be placed anywhere available in the
 *    address space, an unused space will be allocated to fit it in.
 *  - VM_ADDRESS_EXACT: The mapping will be placed at exactly the address
 *    specified, and any existing mappings in the same region will be replaced.
 *  - VM_ADDRESS_HINT: Try to allocate unused space near to the specified
 *    address.
 *
 * The flags argument controls the behaviour of the mapping. The following flags
 * are currently defined:
 *
 *  - VM_MAP_PRIVATE: Modifications to the mapping will not be transferred
 *    through to the source object, and if the address space is duplicated, the
 *    duplicate and original will be given copy-on-write copies of the region.
 *    If this flag is not specified and the address space is duplicated, changes
 *    made in either address space will be visible in the other.
 *  - VM_MAP_OVERCOMMIT: Memory will not be reserved for the mapping, meaning
 *    it can be made larger than the total memory available (memory is only
 *    allocated when it is actually accessed). The default behaviour is to only
 *    allow mappings if the memory requirement can be satisfied.
 *
 * When mapping an object, the calling process must have the correct access
 * rights to the object for the mapping permissions requested.
 *
 * @param _addr         For VM_ADDRESS_ANY, points to a variable in which to
 *                      store the allocated address. For VM_ADDRESS_EXACT,
 *                      points to a variable containing the address to place
 *                      the mapping at.
 * @param size          Size of mapping (multiple of page size).
 * @param align         Alignment of the mapping (power-of-2 greater than or
 *                      equal to the page size, or 0 for any alignment).
 * @param spec          Address specification (VM_ADDRESS_*).
 * @param access        Allowed access flags (VM_ACCESS_*).
 * @param flags         Mapping behaviour flags (VM_MAP_*).
 * @param handle        Handle to object to map in. If INVALID_HANDLE, then
 *                      the region will be an anonymous memory mapping.
 * @param offset        Offset into object to map from (multiple of page size).
 * @param name          Name of the memory mapping, for informational purposes.
 *                      Can be NULL.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_vm_map(
    void **_addr, size_t size, size_t align, unsigned spec, uint32_t access,
    uint32_t flags, handle_t handle, offset_t offset, const char *name)
{
    status_t ret;

    if (!_addr)
        return STATUS_INVALID_ARG;

    ptr_t addr;
    ret = read_user((ptr_t *)_addr, &addr);
    if (ret != STATUS_SUCCESS)
        return ret;

    char *kname = NULL;
    if (name) {
        ret = strndup_from_user(name, REGION_NAME_MAX, &kname);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    object_handle_t *khandle = NULL;
    if (handle != INVALID_HANDLE) {
        ret = object_handle_lookup(handle, -1, &khandle);
        if (ret != STATUS_SUCCESS) {
            kfree(kname);
            return ret;
        }
    }

    ret = vm_map(
        curr_proc->aspace, &addr, size, align, spec, access, flags, khandle,
        offset, kname);
    if (ret == STATUS_SUCCESS) {
        ret = write_user((ptr_t *)_addr, addr);
        if (ret != STATUS_SUCCESS)
            vm_unmap(curr_proc->aspace, addr, size);
    }

    if (khandle)
        object_handle_release(khandle);

    kfree(kname);
    return ret;
}

/**
 * Marks the specified address range as free in the calling process' address
 * space and unmaps anything that may be mapped there.
 *
 * @param start         Start of region to free.
 * @param size          Size of region to free.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_vm_unmap(void *start, size_t size) {
    return vm_unmap(curr_proc->aspace, (ptr_t)start, size);
}
