/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Virtual memory manager.
 */

#ifndef __MM_VM_H
#define __MM_VM_H

#include <arch/memmap.h>
#include <cpu/cpu.h>
#include <mm/page.h>
#include <public/vm.h>
#include <sync/mutex.h>
#include <object.h>

struct device;
struct page_queue;
struct vfs_node;
struct vm_amap;
struct vm_aspace;
struct vm_region;

/** Structure describing a page in memory. */
typedef struct vm_page {
	list_t header;			/**< Link to page queue. */
	struct page_queue *queue;	/**< Queue that the page is in. */

	/** Basic page information. */
	phys_ptr_t addr;		/**< Physical address of the page. */
	bool modified : 1;		/**< Whether the page has been modified. */
	uint8_t unused: 7;

	/** Information about how the page is being used.
	 * @note		Use of count is up to the page owner.
	 * @note		Should not have both object and amap set. */
	refcount_t count;		/**< Reference count of the page (use is up to page user). */
	object_t *object;		/**< Object that the page belongs to. */
	struct vm_amap *amap;		/**< Anonymous map the page belongs to. */
	offset_t offset;		/**< Offset into the object of the page. */
} vm_page_t;

/** Page queue numbers. */
#define PAGE_QUEUE_MODIFIED	0	/**< Pages that need to be written. */
#define PAGE_QUEUE_CACHED	1	/**< Pages that are held in caches. */
#define PAGE_QUEUE_PAGEABLE	2	/**< Pages that are mapped but can be paged out. */
#define PAGE_QUEUE_COUNT	3	/**< Number of page lists. */

/** Structure containing an anonymous memory map. */
typedef struct vm_amap {
	refcount_t count;		/**< Count of regions referring to this object. */
	mutex_t lock;			/**< Lock to protect map. */

	size_t curr_size;		/**< Number of pages currently contained in object. */
	size_t max_size;		/**< Maximum number of pages in object. */
	vm_page_t **pages;		/**< Array of pages currently in object. */
	uint16_t *rref;			/**< Region reference count array. */
} vm_amap_t;

/** Structure representing a region in an address space. */
typedef struct vm_region {
	struct vm_aspace *as;		/**< Address space region belongs to. */
	ptr_t start;			/**< Base address of the region. */
	ptr_t end;			/**< Size of the region. */
	int flags;			/**< Flags for the region. */

	object_handle_t *handle;	/**< Handle to object that this region is mapping. */
	offset_t obj_offset;		/**< Offset into the object. */
	vm_amap_t *amap;		/**< Anonymous map. */
	offset_t amap_offset;		/**< Offset into the anonymous map. */

	avl_tree_node_t *node;		/**< AVL tree node for the region. */
} vm_region_t;

/** Flags for the vm_region_t structure. */
#define VM_REGION_READ		(1<<0)	/**< Region is readable. */
#define VM_REGION_WRITE		(1<<1)	/**< Region is writable. */
#define VM_REGION_EXEC		(1<<2)	/**< Region is executable. */
#define VM_REGION_PRIVATE	(1<<3)	/**< Modifications to this region should not be visible to other processes. */
#define VM_REGION_STACK		(1<<4)	/**< Region contains a stack and should have a guard page. */
#define VM_REGION_RESERVED	(1<<5)	/**< Region is reserved and should never be allocated. */

/** Structure containing a virtual address space. */
typedef struct vm_aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */

	page_map_t pmap;		/**< Underlying page map for address space. */
	avl_tree_t regions;		/**< Tree of mapped regions. */

	vm_region_t *find_cache;	/**< Cached pointer to last region searched for. */
} vm_aspace_t;

/** Macro that expands to a pointer to the current address space. */
#define curr_aspace		(curr_cpu->aspace)

/** Page fault reason codes. */
#define VM_FAULT_NOTPRESENT	1	/**< Fault caused by a not present page. */
#define VM_FAULT_PROTECTION	2	/**< Fault caused by a protection violation. */

/** Page fault access codes.
 * @note		Defined to the same values as the region protection
 *			flags. */
#define VM_FAULT_READ		(1<<0)	/**< Fault caused by a read. */
#define VM_FAULT_WRITE		(1<<1)	/**< Fault caused by a write. */
#define VM_FAULT_EXEC		(1<<2)	/**< Fault when trying to execute. */

extern vm_page_t *vm_page_alloc(size_t count, int pmflag);
extern void vm_page_free(vm_page_t *pages, size_t count);
extern vm_page_t *vm_page_copy(vm_page_t *page, int mmflag);
extern void vm_page_queue(vm_page_t *page, size_t queue);
extern void vm_page_dequeue(vm_page_t *page);
extern vm_page_t *vm_page_lookup(phys_ptr_t addr);

extern bool vm_fault(ptr_t addr, int reason, int access);

extern int vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);
extern int vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags, object_handle_t *handle,
                  offset_t offset, ptr_t *addrp);
extern int vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(void);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_page_init(void);
extern void vm_init(void);

extern int kdbg_cmd_aspace(int argc, char **argv);

#endif /* __MM_VM_H */
