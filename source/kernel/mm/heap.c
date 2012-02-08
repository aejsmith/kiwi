/*
 * Copyright (C) 2008-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel heap allocator.
 *
 * @todo		Dynamic hash table resizing.
 * @todo		Possibly improve SMP scalability? I'm not entirely sure
 *			whether the benefit of doing this would actually be
 *			that great - the majority of heap allocations will take
 *			place from Slab which does per-CPU caching. Need to
 *			investigate this at some point...
 */

#include <arch/memory.h>
#include <arch/page.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/heap.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

#if CONFIG_HEAP_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Number of free lists. */
#define HEAP_FREELISTS			BITS(ptr_t)

/** Initial hash table size. */
#define HEAP_INITIAL_HASH_SIZE		16

/** Depth of a hash chain at which a rehash will be triggered. */
#define HEAP_REHASH_THRESHOLD		32

/** Heap range boundary tag structure. */
typedef struct heap_tag {
	list_t tag_link;		/**< Link to tag list. */
	list_t af_link;			/**< Link to allocated/free lists. */

	ptr_t addr;			/**< Base address of range. */
	size_t size;			/**< Size of the range. */
	bool allocated : 1;		/**< Whether the range is allocated. */
} heap_tag_t;

/** Initial heap hash table. */
static list_t initial_heap_hash[HEAP_INITIAL_HASH_SIZE];

/** Allocation hash table data. */
static list_t *heap_hash = initial_heap_hash;
static size_t heap_hash_size = HEAP_INITIAL_HASH_SIZE;
static bool heap_rehash_requested = false;

/** Free list data. */
static list_t heap_freelists[HEAP_FREELISTS];
static ptr_t heap_freemap = 0;

/** List of all heap ranges. */
static LIST_DECLARE(heap_ranges);

/** Pool of free tags. */
static LIST_DECLARE(heap_tag_pool);

/** Global heap lock. */
static MUTEX_DECLARE(heap_lock, 0);

/** Allocate a new heap tag structure.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to allocated tag on success, null on failure. */
static heap_tag_t *heap_tag_get(int mmflag) {
	heap_tag_t *tag, *first;
	phys_ptr_t page;
	status_t ret;
	size_t i;

	if(unlikely(list_empty(&heap_tag_pool))) {
		/* No free tag structures available. Allocate a new page that
		 * can be accessed from the physical map area. It is expected
		 * that the architecture/platform segregates the free page
		 * lists such that pages accessible through the physical map
		 * area can be allocated using the fast path, and are not
		 * allocated unless pages outside of it aren't available. */
		ret = phys_alloc(PAGE_SIZE, 0, 0, 0, KERNEL_PMAP_SIZE, mmflag & MM_FLAG_MASK, &page);
		if(unlikely(ret != STATUS_SUCCESS)) {
			return NULL;
		}

		/* Split up this page into tag structures. */
		first = NULL;
		for(i = 0; i < (PAGE_SIZE / sizeof(heap_tag_t)); i++) {
			tag = phys_map(page + (i * sizeof(heap_tag_t)), sizeof(heap_tag_t), MM_SLEEP);
			list_init(&tag->tag_link);
			list_init(&tag->af_link);
			tag->allocated = 0;

			if(i == 0) {
				first = tag;
			} else {
				list_append(&heap_tag_pool, &tag->tag_link);
			}
		}

		return first;
	} else {
		/* Pop a tag off the list. */
		tag = list_first(&heap_tag_pool, heap_tag_t, tag_link);
		list_remove(&tag->tag_link);
		return tag;
	}
}

/** Free a heap tag structure.
 * @param tag		Tag to free. */
static inline void heap_tag_put(heap_tag_t *tag) {
	list_append(&heap_tag_pool, &tag->tag_link);
}

/** Insert a tag into the freelist.
 * @param tag		Tag to insert. */
static inline void heap_freelist_insert(heap_tag_t *tag) {
	unsigned list = highbit(tag->size) - 1;

	assert(!tag->allocated);

	list_append(&heap_freelists[list], &tag->af_link);
	heap_freemap |= ((ptr_t)1 << list);
}

/** Remove a tag from the freelist.
 * @param tag		Tag to remove. */
static inline void heap_freelist_remove(heap_tag_t *tag) {
	unsigned list = highbit(tag->size) - 1;

	list_remove(&tag->af_link);
	if(list_empty(&heap_freelists[list])) {
		heap_freemap &= ~((ptr_t)1 << list);
	}
}

/** Find a free range large enough to satisfy an allocation.
 * @param size		Required allocation size.
 * @return		Pointer to tag if found, null if not. */
static inline heap_tag_t *heap_freelist_find(size_t size) {
	unsigned list = highbit(size) - 1, i;
	heap_tag_t *tag;

	/* If the size is exactly a power of 2, then ranges on freelist[n] are
	 * guaranteed to be big enough. Otherwise, use freelist[n + 1] to avoid
	 * the possibility that we have to iterate through multiple ranges on
	 * the list to find one large enough. We only do this if there are
	 * available ranges in higher lists. */
	if((size & (size - 1)) != 0 && heap_freemap >> (list + 1)) {
		list++;
	}

	/* Search through all the lists large enough. */
	for(i = list; i < HEAP_FREELISTS; i++) {
		/* Check if there are any available ranges on this list. */
		if(!(heap_freemap & ((ptr_t)1 << i))) {
			continue;
		}

		assert(!list_empty(&heap_freelists[i]));

		LIST_FOREACH(&heap_freelists[i], iter) {
			tag = list_entry(iter, heap_tag_t, af_link);

			if(tag->size >= size) {
				return tag;
			}
		}
	}

	return NULL;
}

/** Add an allocation to the hash table.
 * @param tag		Tag for the allocation. */
static inline void heap_hash_insert(heap_tag_t *tag) {
	uint32_t hash = fnv_hash_integer(tag->addr) % heap_hash_size;
	list_append(&heap_hash[hash], &tag->af_link);
}

/** Find, check and remove an allocation from the hash table.
 * @param addr		Address of allocation to look up.
 * @param size		Size of allocation to look up.
 * @return		Pointer to allocation tag if found, null if not. */
static heap_tag_t *heap_hash_find(ptr_t addr, size_t size) {
	uint32_t hash = fnv_hash_integer(addr) % heap_hash_size;
	heap_tag_t *tag;
	size_t i = 0;

	assert(size);
	assert(!(addr % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));

	/* Search the bucket for the allocation. */
	LIST_FOREACH(&heap_hash[hash], iter) {
		tag = list_entry(iter, heap_tag_t, af_link);

		assert(tag->allocated);

		if(tag->addr != addr) {
			i++;
			continue;
		}

		/* Although we periodically rehash, if we've exceeded a certain
		 * chain depth in the search for the allocation, trigger a
		 * rehash manually. This is because under heavy load, we don't
		 * want to have to wait for the periodic rehash. */
		if(i >= HEAP_REHASH_THRESHOLD && !heap_rehash_requested) {
			dprintf("heap: saw %zu allocations in search on chain %u, triggering rehash\n",
				i, hash);
			heap_rehash_requested = true;
			// TODO: trigger rehash
		}

		/* Check that it is the expected size. */
		if(unlikely(tag->size != size)) {
			fatal("Incorrect size for heap allocation %p (given: %zu, actual: %zu)",
				addr, size, tag->size);
		}

		/* Erase the allocation and return it. */
		list_remove(&tag->af_link);
		return tag;
	}

	fatal("Cannot find heap allocation %p", addr);
}

/** Allocate a range of unmapped kernel heap.
 * @param size		Size of allocation to make (multiple of page size).
 * @param mmflag	Allocation behaviour flags.
 * @return		Address of allocation on success, 0 on failure. */
ptr_t heap_raw_alloc(size_t size, int mmflag) {
	heap_tag_t *tag, *split;

	assert(size);
	assert(!(size % PAGE_SIZE));

	mutex_lock(&heap_lock);

	/* Find an available free range. */
	tag = heap_freelist_find(size);
	if(unlikely(!tag)) {
		// TODO: Memory reclaim
		if(mmflag & MM_FATAL) {
			fatal("Exhausted kernel heap during boot");
		} else if(mmflag & MM_SLEEP) {
			fatal("Oh god help");
		}

		mutex_unlock(&heap_lock);
		return 0;
	}

	heap_freelist_remove(tag);

	/* Split the tag, if necessary. */
	if(tag->size > size) {
		split = heap_tag_get(mmflag);
		if(!split) {
			heap_freelist_insert(tag);
			mutex_unlock(&heap_lock);
			return 0;
		}

		split->addr = tag->addr + size;
		split->size = tag->size - size;
		list_add_after(&tag->tag_link, &split->tag_link);
		heap_freelist_insert(split);

		tag->size = size;
	}

	/* Mark the tag as allocated, add to the allocation hash table. */
	tag->allocated = true;
	heap_hash_insert(tag);

	dprintf("heap: allocated range [%p,%p)\n", tag->addr, tag->addr + size);
	mutex_unlock(&heap_lock);
	return tag->addr;
}

/** Free a tag and return it to the free list.
 * @param tag		Tag to free. */
static void free_internal(heap_tag_t *tag) {
	heap_tag_t *exist;

	/* Mark the range as free. */
	tag->allocated = false;

	/* Coalesce with adjacent free ranges. */
	if(tag->tag_link.next != &heap_ranges) {
		exist = list_next(&tag->tag_link, heap_tag_t, tag_link);
		if(!exist->allocated) {
			tag->size += exist->size;
			heap_freelist_remove(exist);
			list_remove(&exist->tag_link);
			heap_tag_put(exist);
		}
	}
	if(tag->tag_link.prev != &heap_ranges) {
		exist = list_prev(&tag->tag_link, heap_tag_t, tag_link);
		if(!exist->allocated) {
			tag->addr = exist->addr;
			tag->size += exist->size;
			heap_freelist_remove(exist);
			list_remove(&exist->tag_link);
			heap_tag_put(exist);
		}
	}

	/* Insert the range into the freelist. */
	heap_freelist_insert(tag);
}

/**
 * Free a range of kernel heap.
 *
 * Frees a range of kernel heap without unmapping any pages in the range. This
 * must be done manually before calling this function. The range passed to this
 * function must exactly match the original allocation: you cannot partially
 * free an allocated range.
 *
 * @param addr		Address of range.
 * @param size		Size of range.
 */
void heap_raw_free(ptr_t addr, size_t size) {
	heap_tag_t *tag;

	mutex_lock(&heap_lock);
	tag = heap_hash_find(addr, size);
	free_internal(tag);
	mutex_unlock(&heap_lock);

	dprintf("heap: freed range [%p,%p)\n", addr, addr + size);
}

/** Unmap a range on the kernel heap.
 * @note		Kernel MMU context should be locked.
 * @param start		Start of range.
 * @param end		End of range.
 * @param free		Whether to free the pages.
 * @param shared	Whether the mapping was shared with other CPUs. */
static void unmap_range(ptr_t start, ptr_t end, bool free, bool shared) {
	phys_ptr_t page;
	ptr_t i;

	for(i = start; i < end; i += PAGE_SIZE) {
		if(!mmu_context_unmap(&kernel_mmu_context, i, shared, &page)) {
			fatal("Address %p was not mapped while freeing", i);
		}
		if(free) {
			phys_free(page, PAGE_SIZE);
		}

		dprintf("heap: unmapped page 0x%" PRIxPHYS " from %p\n", page, i);
	}
}

/**
 * Allocate a range of kernel heap.
 *
 * Allocates a range of kernel heap and backs it with anonymous pages from the
 * physical memory manager. All pages required to cover the range are allocated
 * immediately, so this function should not be used to make very large
 * allocations. The allocated pages are not contiguous in physical memory.
 *
 * @param size		Size of allocation to make (multiple of page size).
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Address of allocation on success, 0 on failure.
 */
void *heap_alloc(size_t size, int mmflag) {
	status_t ret;
	page_t *page;
	ptr_t addr;
	size_t i;

	/* Allocate a range to map into. */
	addr = heap_raw_alloc(size, mmflag);
	if(unlikely(!addr)) {
		return NULL;
	}

	mmu_context_lock(&kernel_mmu_context);

	/* Back the allocation with anonymous pages. */
	for(i = 0; i < size; i += PAGE_SIZE) {
		page = page_alloc(mmflag & MM_FLAG_MASK);
		if(unlikely(!page)) {
			kprintf(LOG_DEBUG, "heap: unable to allocate pages to back allocation\n");
			goto fail;
		}

		/* Map the page into the kernel address space. */
		ret = mmu_context_map(&kernel_mmu_context, addr + i, page->addr,
		                      true, true, mmflag & MM_FLAG_MASK);
		if(unlikely(ret != STATUS_SUCCESS)) {
			kprintf(LOG_DEBUG, "heap: failed to map page 0x%" PRIxPHYS " to %p (%d)\n",
			        page->addr, addr + i, ret);
			page_free(page);
			goto fail;
		}

		dprintf("heap: mapped page 0x%" PRIxPHYS " at %p\n", page->addr, addr + i);
	}

	mmu_context_unlock(&kernel_mmu_context);
	return (void *)addr;
fail:
	/* Go back and reverse what we have done. */
	unmap_range(ret, ret + i, true, true);
	mmu_context_unlock(&kernel_mmu_context);
	heap_raw_free(addr, size);
	return NULL;
}

/**
 * Free a range of kernel heap.
 *
 * Unmaps and frees all pages covering a range of kernel heap and frees the
 * range. The range passed to this function must exactly match the original
 * allocation: you cannot partially free an allocated range.
 *
 * @param addr		Address of range.
 * @param size		Size of range.
 */
void heap_free(void *addr, size_t size) {
	heap_tag_t *tag;

	/* Get the allocation and take it off the hash table. */
	mutex_lock(&heap_lock);
	tag = heap_hash_find((ptr_t)addr, size);
	mutex_unlock(&heap_lock);

	/* Unmap pages covering the range. */
	mmu_context_lock(&kernel_mmu_context);
	unmap_range((ptr_t)addr, (ptr_t)addr + size, true, true);
	mmu_context_unlock(&kernel_mmu_context);

	/* Free it. */
	mutex_lock(&heap_lock);
	free_internal(tag);
	mutex_unlock(&heap_lock);

	dprintf("heap: freed range [%p,%p)\n", addr, (ptr_t)addr + size);
}

/**
 * Map a range of pages on the kernel heap.
 *
 * Allocates space on the kernel heap and maps the specified page range into
 * it. The mapping must later be unmapped and freed using heap_unmap_range().
 *
 * @param base		Base address of the page range.
 * @param size		Size of range to map (must be multiple of PAGE_SIZE).
 * @param mmflag	Allocation flags.
 *
 * @return		Pointer to mapped range.
 */
void *heap_map_range(phys_ptr_t base, size_t size, int mmflag) {
	ptr_t ret;
	size_t i;

	assert(!(base % PAGE_SIZE));

	ret = heap_raw_alloc(size, mmflag);
	if(unlikely(!ret)) {
		return NULL;
	}

	mmu_context_lock(&kernel_mmu_context);

	/* Back the allocation with the required page range. */
	for(i = 0; i < size; i += PAGE_SIZE) {
		if(mmu_context_map(&kernel_mmu_context, ret + i, base + i, true, true,
		                   mmflag & MM_FLAG_MASK) != STATUS_SUCCESS) {
			kprintf(LOG_DEBUG, "heap: failed to map page 0x%" PRIxPHYS " to %p\n", base, ret + i);
			goto fail;
		}

		dprintf("heap: mapped page 0x%" PRIxPHYS " at %p\n", base, ret + i);
	}

	mmu_context_unlock(&kernel_mmu_context);
	return (void *)ret;
fail:
	/* Go back and reverse what we have done. */
	unmap_range(ret, ret + i, true, true);
	mmu_context_unlock(&kernel_mmu_context);
	heap_raw_free(ret, size);
	return NULL;
}

/**
 * Unmap a range of pages on the kernel heap.
 *
 * Unmaps a range of pages on the kernel heap and frees the space used by the
 * range. The range should have previously been allocated using
 * heap_map_range(), and the number of pages to unmap should match the size
 * of the original allocation.
 *
 * @param addr		Address to free.
 * @param size		Size of range to unmap (must be multiple of PAGE_SIZE).
 * @param shared	Whether the mapping was used by any other CPUs. This
 *			is used as an optimization to reduce the number of
 *			remote TLB invalidations we have to do when doing
 *			physical mappings.
 */
void heap_unmap_range(void *addr, size_t size, bool shared) {
	heap_tag_t *tag;

	/* Get the allocation and take it off the hash table. */
	mutex_lock(&heap_lock);
	tag = heap_hash_find((ptr_t)addr, size);
	mutex_unlock(&heap_lock);

	/* Unmap pages covering the range. */
	mmu_context_lock(&kernel_mmu_context);
	unmap_range((ptr_t)addr, (ptr_t)addr + size, false, true);
	mmu_context_unlock(&kernel_mmu_context);

	/* Free it. */
	mutex_lock(&heap_lock);
	free_internal(tag);
	mutex_unlock(&heap_lock);
}

/** Initialise the kernel heap allocator. */
__init_text void heap_init(void) {
	heap_tag_t *tag;
	unsigned i;

	/* Initialise lists. */
	for(i = 0; i < HEAP_INITIAL_HASH_SIZE; i++) {
		list_init(&initial_heap_hash[i]);
	}
	for(i = 0; i < HEAP_FREELISTS; i++) {
		list_init(&heap_freelists[i]);
	}

	/* Create the initial free range. */
	tag = heap_tag_get(MM_FATAL);
	tag->addr = KERNEL_HEAP_BASE;
	tag->size = KERNEL_HEAP_SIZE;
	list_append(&heap_ranges, &tag->tag_link);
	heap_freelist_insert(tag);
}
