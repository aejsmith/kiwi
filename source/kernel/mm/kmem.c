/*
 * Copyright (C) 2011-2012 Alex Smith
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
 * @brief		Kernel virtual memory allocator.
 *
 * @todo		Dynamic hash table resizing.
 * @todo		Possibly improve SMP scalability? I'm not entirely sure
 *			whether the benefit of doing this would actually be
 *			that great - the majority of kernel memory allocations
 *			take place from Slab which does per-CPU caching. Need
 *			to investigate this at some point...
 */

#include <arch/page.h>

#include <lib/fnv.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/kmem.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

#if CONFIG_KMEM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Number of free lists. */
#define KMEM_FREELISTS			BITS(ptr_t)

/** Initial hash table size. */
#define KMEM_INITIAL_HASH_SIZE		16

/** Depth of a hash chain at which a rehash will be triggered. */
#define KMEM_REHASH_THRESHOLD		32

/** Kernel memory range structure. */
typedef struct kmem_range {
	list_t range_link;		/**< Link to range list. */
	list_t af_link;			/**< Link to allocated/free lists. */

	ptr_t addr;			/**< Base address of range. */
	size_t size;			/**< Size of the range. */
	bool allocated : 1;		/**< Whether the range is allocated. */
} kmem_range_t;

/** Initial allocation hash table. */
static list_t kmem_initial_hash[KMEM_INITIAL_HASH_SIZE];

/** Allocation hash table data. */
static list_t *kmem_hash = kmem_initial_hash;
static size_t kmem_hash_size = KMEM_INITIAL_HASH_SIZE;
static bool kmem_rehash_requested = false;

/** Free range list data. */
static list_t kmem_freelists[KMEM_FREELISTS];
static ptr_t kmem_freemap = 0;

/** Sorted list of all kernel memory ranges. */
static LIST_DEFINE(kmem_ranges);

/** Pool of free range structures. */
static LIST_DEFINE(kmem_range_pool);

/** Global kernel memory lock. */
static MUTEX_DEFINE(kmem_lock, 0);

/** Allocate a new range structure.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to allocated structure on success, NULL on
 *			failure. */
static kmem_range_t *kmem_range_get(unsigned mmflag) {
	kmem_range_t *range, *first;
	phys_ptr_t page;
	status_t ret;
	size_t i;

	if(unlikely(list_empty(&kmem_range_pool))) {
		/* No free range structures available. Allocate a new page that
		 * can be accessed from the physical map area. It is expected
		 * that the architecture/platform segregates the free page
		 * lists such that pages accessible through the physical map
		 * area can be allocated using the fast path, and are not
		 * allocated unless pages outside of it aren't available. */
		ret = phys_alloc(PAGE_SIZE, 0, 0, KERNEL_PMAP_OFFSET,
			KERNEL_PMAP_OFFSET + KERNEL_PMAP_SIZE,
			mmflag & MM_FLAG_MASK, &page);
		if(unlikely(ret != STATUS_SUCCESS)) {
			return NULL;
		}

		/* Split up this page into range structures. */
		first = NULL;
		for(i = 0; i < (PAGE_SIZE / sizeof(kmem_range_t)); i++) {
			range = phys_map(page + (i * sizeof(kmem_range_t)),
				sizeof(kmem_range_t), MM_KERNEL);
			list_init(&range->range_link);
			list_init(&range->af_link);
			range->allocated = 0;

			if(i == 0) {
				first = range;
			} else {
				list_append(&kmem_range_pool, &range->range_link);
			}
		}

		return first;
	} else {
		/* Pop a structure off the list. */
		range = list_first(&kmem_range_pool, kmem_range_t, range_link);
		list_remove(&range->range_link);
		return range;
	}
}

/** Free a range structure.
 * @param range		Structure to free. */
static inline void kmem_range_put(kmem_range_t *range) {
	list_append(&kmem_range_pool, &range->range_link);
}

/** Insert a range into the freelist.
 * @param range		Range to insert. */
static inline void kmem_freelist_insert(kmem_range_t *range) {
	unsigned list = highbit(range->size) - 1;

	assert(!range->allocated);

	list_append(&kmem_freelists[list], &range->af_link);
	kmem_freemap |= ((ptr_t)1 << list);
}

/** Remove a range from the freelist.
 * @param range		Range to remove. */
static inline void kmem_freelist_remove(kmem_range_t *range) {
	unsigned list = highbit(range->size) - 1;

	list_remove(&range->af_link);
	if(list_empty(&kmem_freelists[list]))
		kmem_freemap &= ~((ptr_t)1 << list);
}

/** Find a free range large enough to satisfy an allocation.
 * @param size		Required allocation size.
 * @return		Pointer to range if found, NULL if not. */
static inline kmem_range_t *kmem_freelist_find(size_t size) {
	unsigned list = highbit(size) - 1, i;
	kmem_range_t *range;

	/* If the size is exactly a power of 2, then ranges on freelist[n] are
	 * guaranteed to be big enough. Otherwise, use freelist[n + 1] to avoid
	 * the possibility that we have to iterate through multiple ranges on
	 * the list to find one large enough. We only do this if there are
	 * available ranges in higher lists. */
	if((size & (size - 1)) != 0 && kmem_freemap >> (list + 1))
		list++;

	/* Search through all the lists large enough. */
	for(i = list; i < KMEM_FREELISTS; i++) {
		/* Check if there are any available ranges on this list. */
		if(!(kmem_freemap & ((ptr_t)1 << i)))
			continue;

		assert(!list_empty(&kmem_freelists[i]));

		LIST_FOREACH(&kmem_freelists[i], iter) {
			range = list_entry(iter, kmem_range_t, af_link);

			if(range->size >= size)
				return range;
		}
	}

	return NULL;
}

/** Add an allocation to the hash table.
 * @param range		Range to insert. */
static inline void kmem_hash_insert(kmem_range_t *range) {
	uint32_t hash = fnv_hash_integer(range->addr) % kmem_hash_size;
	list_append(&kmem_hash[hash], &range->af_link);
}

/** Find an allocation from the hash table.
 * @param addr		Address of allocation to look up.
 * @param size		Size of allocation to look up.
 * @return		Pointer to range structure for allocation. */
static kmem_range_t *kmem_hash_find(ptr_t addr, size_t size) {
	uint32_t hash = fnv_hash_integer(addr) % kmem_hash_size;
	kmem_range_t *range;
	size_t i = 0;

	assert(size);
	assert(!(addr % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));

	/* Search the bucket for the allocation. */
	LIST_FOREACH(&kmem_hash[hash], iter) {
		range = list_entry(iter, kmem_range_t, af_link);

		assert(range->allocated);

		if(range->addr != addr) {
			i++;
			continue;
		}

		/* Although we periodically rehash, if we've exceeded a certain
		 * chain depth in the search for the allocation, trigger a
		 * rehash manually. This is because under heavy load, we don't
		 * want to have to wait for the periodic rehash. */
		if(i >= KMEM_REHASH_THRESHOLD && !kmem_rehash_requested) {
			dprintf("kmem: saw %zu allocations in search on chain %u, triggering rehash\n",
				i, hash);
			kmem_rehash_requested = true;
			// TODO: trigger rehash
		}

		return range;
	}

	return NULL;
}

/** Internal part of the freeing functions.
 * @param addr		Address to free.
 * @param size		Size of the range to free.
 * @param unmap		Whether to unmap the range.
 * @param free		Whether to free pages backing the range.
 * @param shared	Whether the mapping was shared with other CPUs. */
static void kmem_free_internal(ptr_t addr, size_t size, bool unmap, bool free, bool shared) {
	kmem_range_t *range, *exist;
	page_t *page;
	size_t i;

	mutex_lock(&kmem_lock);

	/* Search for the allocation and check if it is as expected. */
	range = kmem_hash_find(addr, size);
	if(unlikely(!range)) {
		fatal("Invalid free of %p", addr);
	} else if(unlikely(range->size != size)) {
		fatal("Incorrect size for allocation %p (given: %zu, actual: %zu)",
			addr, size, range->size);
	}

	/* Remove it from the hash table. */
	list_remove(&range->af_link);

	mutex_unlock(&kmem_lock);

	/* Unmap pages covering the range. */
	if(unmap) {
		mmu_context_lock(&kernel_mmu_context);

		for(i = 0; i < size; i += PAGE_SIZE) {
			if(!mmu_context_unmap(&kernel_mmu_context, addr + i, shared, &page))
				fatal("Address %p was not mapped while freeing", addr + i);

			if(free)
				page_free(page);

			dprintf("kmem: unmapped page 0x%" PRIxPHYS " from %p\n", page, addr + i);
		}

		mmu_context_unlock(&kernel_mmu_context);
	}

	mutex_lock(&kmem_lock);

	/* Mark the range as free. */
	range->allocated = false;

	/* Coalesce with adjacent free ranges. */
	if(range != list_last(&kmem_ranges, kmem_range_t, range_link)) {
		exist = list_next(range, range_link);
		if(!exist->allocated) {
			range->size += exist->size;
			kmem_freelist_remove(exist);
			list_remove(&exist->range_link);
			kmem_range_put(exist);
		}
	}
	if(range != list_first(&kmem_ranges, kmem_range_t, range_link)) {
		exist = list_prev(range, range_link);
		if(!exist->allocated) {
			range->addr = exist->addr;
			range->size += exist->size;
			kmem_freelist_remove(exist);
			list_remove(&exist->range_link);
			kmem_range_put(exist);
		}
	}

	/* Insert the range into the freelist. */
	kmem_freelist_insert(range);

	mutex_unlock(&kmem_lock);

	dprintf("kmem: freed range [%p,%p)\n", addr, (ptr_t)addr + size);
}

/** Allocate a range of unmapped kernel memory.
 * @param size		Size of allocation to make (multiple of PAGE_SIZE).
 * @param mmflag	Allocation behaviour flags.
 * @return		Address of allocation on success, 0 on failure. */
ptr_t kmem_raw_alloc(size_t size, unsigned mmflag) {
	kmem_range_t *range, *split;

	assert(size);
	assert(!(size % PAGE_SIZE));
	assert((mmflag & (MM_WAIT | MM_ATOMIC)) != (MM_WAIT | MM_ATOMIC));

	mutex_lock(&kmem_lock);

	/* Find an available free range. */
	range = kmem_freelist_find(size);
	if(unlikely(!range)) {
		// TODO: Reclaim/wait for memory.
		if(mmflag & MM_BOOT) {
			fatal("Exhausted kernel memory during boot");
		} else if(mmflag & MM_WAIT) {
			fatal("TODO: Reclaim/wait for memory");
		}

		mutex_unlock(&kmem_lock);
		return 0;
	}

	kmem_freelist_remove(range);

	/* Split the range, if necessary. */
	if(range->size > size) {
		split = kmem_range_get(mmflag);
		if(!split) {
			kmem_freelist_insert(range);
			mutex_unlock(&kmem_lock);
			return 0;
		}

		split->addr = range->addr + size;
		split->size = range->size - size;
		list_add_after(&range->range_link, &split->range_link);
		kmem_freelist_insert(split);

		range->size = size;
	}

	/* Mark the range as allocated, add to the allocation hash table. */
	range->allocated = true;
	kmem_hash_insert(range);

	dprintf("kmem: allocated range [%p,%p)\n", range->addr, range->addr + size);
	mutex_unlock(&kmem_lock);
	return range->addr;
}

/**
 * Free a range of kernel memory.
 *
 * Frees a range of kernel memory without unmapping any pages in the range.
 * This must be done manually before calling this function. The range passed
 * to this function must exactly match the original allocation: you cannot
 * partially free an allocated range.
 *
 * @param addr		Address of range.
 * @param size		Size of range.
 */
void kmem_raw_free(ptr_t addr, size_t size) {
	kmem_free_internal((ptr_t)addr, size, false, false, false);
}

/**
 * Allocate a range of kernel memory.
 *
 * Allocates a range of kernel memory and backs it with anonymous pages from
 * the physical memory manager. All pages required to cover the range are
 * allocated immediately, so this function should not be used to make very
 * large allocations. The allocated pages are not contiguous in physical
 * memory.
 *
 * @param size		Size of allocation to make (multiple of page size).
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Address of allocation on success, 0 on failure.
 */
void *kmem_alloc(size_t size, unsigned mmflag) {
	page_t *page;
	ptr_t addr;
	size_t i;
	status_t ret;

	/* Allocate a range to map into. */
	addr = kmem_raw_alloc(size, mmflag);
	if(unlikely(!addr))
		return NULL;

	mmu_context_lock(&kernel_mmu_context);

	/* Back the allocation with anonymous pages. */
	for(i = 0; i < size; i += PAGE_SIZE) {
		page = page_alloc(mmflag & MM_FLAG_MASK);
		if(unlikely(!page)) {
			kprintf(LOG_DEBUG, "kmem: unable to allocate pages to back allocation\n");
			goto fail;
		}

		/* Map the page into the kernel address space. */
		ret = mmu_context_map(&kernel_mmu_context, addr + i, page->addr,
			VM_ACCESS_READ | VM_ACCESS_WRITE | VM_ACCESS_EXECUTE,
			mmflag & MM_FLAG_MASK);
		if(ret != STATUS_SUCCESS) {
			kprintf(LOG_DEBUG, "kmem: failed to map page 0x%" PRIxPHYS " to %p\n",
				page->addr, addr + i);
			page_free(page);
			goto fail;
		}

		dprintf("kmem: mapped page 0x%" PRIxPHYS " at %p\n", page->addr, addr + i);
	}

	/* Zero the range if requested. */
	if(mmflag & MM_ZERO)
		memset((void *)addr, 0, size);

	mmu_context_unlock(&kernel_mmu_context);
	return (void *)addr;
fail:
	/* Go back and reverse what we have done. */
	for(; i; i -= PAGE_SIZE) {
		mmu_context_unmap(&kernel_mmu_context, addr + (i - PAGE_SIZE), true, &page);
		page_free(page);
	}
	mmu_context_unlock(&kernel_mmu_context);
	kmem_raw_free(addr, size);
	return NULL;
}

/**
 * Free a range of kernel memory.
 *
 * Unmaps and frees all pages covering a range of kernel memory and frees the
 * range. The range passed to this function must exactly match the original
 * allocation: you cannot partially free an allocated range.
 *
 * @param addr		Address of range.
 * @param size		Size of range.
 */
void kmem_free(void *addr, size_t size) {
	kmem_free_internal((ptr_t)addr, size, true, true, true);
}

/**
 * Map a range of pages in kernel memory.
 *
 * Allocates kernel memory space and maps the specified page range into it.
 * The mapping must later be unmapped and freed using kmem_unmap(). For
 * mapping physical memory, phys_map() should be used instead, as it will
 * use the physical map area where possible.
 *
 * @param base		Base address of the page range.
 * @param size		Size of range to map (must be multiple of PAGE_SIZE).
 * @param mmflag	Allocation flags.
 *
 * @return		Pointer to mapped range.
 */
void *kmem_map(phys_ptr_t base, size_t size, unsigned mmflag) {
	ptr_t addr;
	size_t i;
	status_t ret;

	assert(!(base % PAGE_SIZE));

	addr = kmem_raw_alloc(size, mmflag);
	if(unlikely(!addr))
		return NULL;

	mmu_context_lock(&kernel_mmu_context);

	/* Back the allocation with the required page range. */
	for(i = 0; i < size; i += PAGE_SIZE) {
		ret = mmu_context_map(&kernel_mmu_context, addr + i, base + i,
			VM_ACCESS_READ | VM_ACCESS_WRITE | VM_ACCESS_EXECUTE,
			mmflag & MM_FLAG_MASK);
		if(ret != STATUS_SUCCESS) {
			kprintf(LOG_DEBUG, "kmem: failed to map page 0x%" PRIxPHYS " to %p\n",
				base + i, addr + i);
			goto fail;
		}

		dprintf("kmem: mapped page 0x%" PRIxPHYS " at %p\n", base + i,
			addr + i);
	}

	mmu_context_unlock(&kernel_mmu_context);
	return (void *)addr;
fail:
	/* Go back and reverse what we have done. */
	for(; i; i -= PAGE_SIZE) {
		mmu_context_unmap(&kernel_mmu_context, addr + (i - PAGE_SIZE),
			true, NULL);
	}

	mmu_context_unlock(&kernel_mmu_context);
	kmem_raw_free(addr, size);
	return NULL;
}

/**
 * Unmap a range of pages from kernel memory.
 *
 * Unmaps a range of pages from kernel memory and frees the space used by the
 * range. The range should have previously been mapped using kmem_map(), and
 * the number of pages to unmap should match the size of the original
 * allocation.
 *
 * @param addr		Address to free.
 * @param size		Size of range to unmap (must be multiple of PAGE_SIZE).
 * @param shared	Whether the mapping was used by any other CPUs. This
 *			is used as an optimization to reduce the number of
 *			remote TLB invalidations we have to do when doing
 *			physical mappings.
 */
void kmem_unmap(void *addr, size_t size, bool shared) {
	kmem_free_internal((ptr_t)addr, size, true, false, shared);
}

/** Initialize the kernel memory allocator. */
__init_text void kmem_init(void) {
	kmem_range_t *range;
	ptr_t start, end, boot_end;
	unsigned i;

	/* Initialize lists. */
	for(i = 0; i < KMEM_INITIAL_HASH_SIZE; i++)
		list_init(&kmem_initial_hash[i]);
	for(i = 0; i < KMEM_FREELISTS; i++)
		list_init(&kmem_freelists[i]);

	/* We need to account for all of the boot allocations. To do this, we
	 * just make a single chunk that covers all of the allocations (they
	 * are contiguous). This gets freed later by kmem_late_init(). */
	boot_end = KERNEL_KMEM_BASE;
	KBOOT_ITERATE(KBOOT_TAG_VMEM, kboot_tag_vmem_t, range) {
		start = range->start;
		end = range->start + range->size;

		/* Only want to include ranges in kmem space. */
		if(start < KERNEL_KMEM_BASE || end - 1 > KERNEL_KMEM_END)
			continue;

		if(start != boot_end)
			fatal("Cannot handle non-contiguous KBoot virtual ranges");

		boot_end = end;
	}

	if(boot_end != KERNEL_KMEM_BASE) {
		range = kmem_range_get(MM_BOOT);
		range->addr = KERNEL_KMEM_BASE;
		range->size = boot_end - KERNEL_KMEM_BASE;
		range->allocated = true;
		list_append(&kmem_ranges, &range->range_link);

		/* Put it on the hash table so we can just free it with a call
		 * to kmem_free(). */
		kmem_hash_insert(range);
	}

	/* Create the initial free range. */
	range = kmem_range_get(MM_BOOT);
	range->addr = boot_end;
	range->size = KERNEL_KMEM_SIZE - (boot_end - KERNEL_KMEM_BASE);
	list_append(&kmem_ranges, &range->range_link);
	kmem_freelist_insert(range);
}

/** Free up space taken by boot mappings. */
__init_text void kmem_late_init(void) {
	ptr_t start, end, boot_end;

	/* Find out the boot mapping end again. We're actually accessing free
	 * pages here (we're called after page_late_init()), but nothing should
	 * have touched them since freeing them. */
	boot_end = KERNEL_KMEM_BASE;
	KBOOT_ITERATE(KBOOT_TAG_VMEM, kboot_tag_vmem_t, range) {
		start = range->start;
		end = range->start + range->size;

		/* Only want to include ranges in kmem space. */
		if(start < KERNEL_KMEM_BASE || end - 1 > KERNEL_KMEM_END)
			continue;

		boot_end = end;
	}

	if(boot_end != KERNEL_KMEM_BASE) {
		/* The pages have already been freed, so we don't want to free
		 * them again, but do need to unmap them. */
		kmem_unmap((void *)KERNEL_KMEM_BASE, boot_end - KERNEL_KMEM_BASE, true);
	}
}
