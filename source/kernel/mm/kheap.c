/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Kernel heap manager.
 *
 * The kernel heap manager uses vmem to manage the kernel heap. It uses three
 * levels of arenas, listed below:
 *  - kheap_raw_arena: This allocates address ranges on the heap.
 *  - kheap_va_arena:  This uses kheap_raw_arena as its source and provides
 *                     quantum caching over it.
 *  - kheap_arena:     This uses kheap_va_arena as its source and backs ranges
 *                     allocated from it with anonymous pages.
 * You might be wondering why we don't just provide the quantum caching on
 * kheap_raw_arena. The slab allocator, which provides the quantum caching
 * functionality, requires memory to stores its structures in. It cannot use
 * an arena with quantum caching to get these, because it would end up
 * recursively allocating. Therefore, it uses its own arena similar to
 * kheap_arena that bypasses kheap_va_arena and thus the quantum caching it
 * provides.
 *
 * To initialise the heap allocator, we must first initialise the raw heap
 * arena, which is performed in kheap_early_init(). Then, slab_init() is
 * called to set up the slab allocator's internal arenas and caches. Finally,
 * kheap_init() is called which sets up kheap_va_arena and kheap_arena.
 */

#include <arch/memory.h>
#include <arch/page.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <status.h>

#if CONFIG_KHEAP_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Kernel heap arenas. */
vmem_t kheap_raw_arena;		/**< Raw heap address allocator. */
vmem_t kheap_va_arena;		/**< Allocator that provides quantum caching. */
vmem_t kheap_arena;		/**< Allocator that backs allocated ranges with anonymous pages. */

/** Unmap a range on the kernel heap.
 * @note		Kernel page map should be locked.
 * @param start		Start of range.
 * @param end		End of range.
 * @param free		Whether to free the pages.
 * @param shared	Whether the mapping was shared with other CPUs. */
static void kheap_do_unmap(ptr_t start, ptr_t end, bool free, bool shared) {
	phys_ptr_t page;
	ptr_t i;

	for(i = start; i < end; i += PAGE_SIZE) {
		if(!page_map_remove(&kernel_page_map, i, shared, &page)) {
			fatal("Address %p was not mapped while freeing", i);
		}
		if(free) {
			page_free(page, 1);
		}

		dprintf("kheap: unmapped page 0x%" PRIpp " from %p\n", page, i);
	}
}

/** Kernel heap arena import callback.
 * @param base		Base of allocation.
 * @param size		Size of allocation.
 * @param vmflag	Allocation flags.
 * @return		Status code describing result of the operation. */
status_t kheap_anon_import(vmem_resource_t base, vmem_resource_t size, int vmflag) {
	vmem_resource_t i;
	phys_ptr_t page;
	status_t ret;

	page_map_lock(&kernel_page_map);

	/* Back the allocation with anonymous pages. */
	for(i = 0; i < size; i += PAGE_SIZE) {
		page = page_alloc(1, vmflag & MM_FLAG_MASK);
		if(unlikely(!page)) {
			dprintf("kheap: unable to allocate pages to back allocation\n");
			goto fail;
		}

		/* Map the page into the kernel address space. */
		ret = page_map_insert(&kernel_page_map, (ptr_t)(base + i), page,
		                      true, true, vmflag & MM_FLAG_MASK);
		if(unlikely(ret != STATUS_SUCCESS)) {
			dprintf("kheap: failed to map page 0x%" PRIpp " to %p (%d)\n",
			        page, (ptr_t)(base + i), ret);
			page_free(page, 1);
			goto fail;
		}

		dprintf("kheap: mapped page 0x%" PRIpp " at %p\n", page, ret + i);
	}

	page_map_unlock(&kernel_page_map);
	return STATUS_SUCCESS;
fail:
	/* Go back and reverse what we have done. */
	kheap_do_unmap(ret, ret + i, true, true);
	page_map_unlock(&kernel_page_map);
	return 0;
}

/** Kernel heap arena release callback.
 * @param base		Base of range to free.
 * @param size		Size of range to free. */
void kheap_anon_release(vmem_resource_t base, vmem_resource_t size) {
	page_map_lock(&kernel_page_map);
	kheap_do_unmap((ptr_t)base, (ptr_t)(base + size), true, true);
	page_map_unlock(&kernel_page_map);
}

/** Allocate from the kernel heap.
 *
 * Allocates a range from the kernel heap and backs it with anonymous pages
 * from the physical memory manager.
 *
 * @param size		Size of the allocation.
 * @param vmflag	Allocation flags.
 *
 * @return		Address allocated or NULL if no available memory.
 */
void *kheap_alloc(size_t size, int vmflag) {
	return (void *)((ptr_t)vmem_alloc(&kheap_arena, size, vmflag));
}

/** Free a previous allocation from the kernel heap.
 *
 * Frees a previously allocated range in the kernel heap. The size specified
 * must be the size of the original allocation. Will free all pages backing
 * the range, therefore it is advised that this only be used when the original
 * allocation was done kheap_alloc(). If it was done with kheap_map_range(),
 * you should use kheap_unmap_range().
 *
 * @param addr		Address to free.
 * @param size		Size of the original allocation.
 */
void kheap_free(void *addr, size_t size) {
	vmem_free(&kheap_arena, (vmem_resource_t)((ptr_t)addr), size);
}

/** Map a range of pages on the kernel heap.
 *
 * Allocates space on the kernel heap and maps the specified page range into
 * it. The mapping must later be unmapped and freed using kheap_unmap_range().
 *
 * @param base		Base address of the page range.
 * @param size		Size of range to map (must be multiple of PAGE_SIZE).
 * @param vmflag	Allocation flags.
 *
 * @return		Pointer to mapped range.
 */
void *kheap_map_range(phys_ptr_t base, size_t size, int vmflag) {
	ptr_t ret;
	size_t i;

	assert(!(base % PAGE_SIZE));

	ret = (ptr_t)vmem_alloc(&kheap_va_arena, size, vmflag);
	if(unlikely(!ret)) {
		return NULL;
	}

	page_map_lock(&kernel_page_map);

	/* Back the allocation with the required page range. */
	for(i = 0; i < size; i += PAGE_SIZE, base += PAGE_SIZE) {
		if(page_map_insert(&kernel_page_map, ret + i, base, true, true,
		                   vmflag & MM_FLAG_MASK) != STATUS_SUCCESS) {
			dprintf("kheap: failed to map page 0x%" PRIpp " to %p\n", base, ret + i);
			goto fail;
		}

		dprintf("kheap: mapped page 0x%" PRIpp " at %p\n", base, ret + i);
	}

	page_map_unlock(&kernel_page_map);
	return (void *)ret;
fail:
	/* Go back and reverse what we have done. */
	kheap_do_unmap(ret, ret + i, true, true);
	page_map_unlock(&kernel_page_map);
	vmem_free(&kheap_va_arena, ret, size);
	return NULL;
}

/** Unmap a range of pages on the kernel heap.
 *
 * Unmaps a range of pages on the kernel heap and frees the space used by the
 * range. The range should have previously been allocated using
 * kheap_map_range(), and the number of pages to unmap should match the size
 * of the original allocation.
 *
 * @param addr		Address to free.
 * @param size		Size of range to unmap (must be multiple of PAGE_SIZE).
 * @param shared	Whether the mapping was used by any other CPUs. This
 *			is used as an optimization to reduce the number of
 *			remote TLB invalidations we have to do when doing
 *			physical mappings.
 */
void kheap_unmap_range(void *addr, size_t size, bool shared) {
	page_map_lock(&kernel_page_map);
	kheap_do_unmap((ptr_t)addr, (ptr_t)addr + size, false, shared);
	page_map_unlock(&kernel_page_map);
	vmem_free(&kheap_va_arena, (ptr_t)addr, size);
}

/** First part of kernel heap initialisation. */
void __init_text kheap_early_init(void) {
	vmem_early_create(&kheap_raw_arena, "kheap_raw_arena", PAGE_SIZE, RESOURCE_TYPE_KASPACE,
	                  VMEM_REFILL, NULL, NULL, NULL, 0, KERNEL_HEAP_BASE, KERNEL_HEAP_SIZE,
	                  MM_FATAL);
}

/** Second part of heap initialisation. */
void __init_text kheap_init(void) {
	vmem_early_create(&kheap_va_arena, "kheap_va_arena", PAGE_SIZE, 0, 0,
	                  &kheap_raw_arena, NULL, NULL, PAGE_SIZE * 8, 0, 0,
	                  MM_FATAL);
	vmem_early_create(&kheap_arena, "kheap_arena", PAGE_SIZE, 0, 0,
	                  &kheap_va_arena, kheap_anon_import, kheap_anon_release,
	                  0, 0, 0, MM_FATAL);
}
