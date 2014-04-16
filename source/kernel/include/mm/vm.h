/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Virtual memory manager.
 */

#ifndef __MM_VM_H
#define __MM_VM_H

#include <arch/page.h>

#include <kernel/vm.h>

#include <lib/avl_tree.h>
#include <lib/utility.h>

#include <mm/page.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <cpu.h>
#include <object.h>

struct intr_frame;
struct mmu_context;
struct vm_aspace;
struct vm_region;

/** Number of free lists to use. */
#define VM_FREELISTS		(((unsigned)BITS(ptr_t)) - PAGE_WIDTH)

/** Maximum length of a region name. */
#define REGION_NAME_MAX		32

/**
 * Interface provided by an object to access its pages.
 *
 * This structure contains operations used by the VM to access an object's
 * pages. When an object is mapped through a handle, the VM calls the object
 * type's map() method. That method is expected to check that the requested
 * access is allowed, and then either map the entire region up front, or set
 * the region's "ops" and "private" pointers. The private pointer will be passed
 * to all of these functions.
 */
typedef struct vm_region_ops {
	/** Get a page for the region.
	 * @param region	Region to get page for.
	 * @param offset	Offset into object to get page from.
	 * @param pagep		Where to store pointer to page structure.
	 * @return		Status code describing result of the operation. */
	status_t (*get_page)(struct vm_region *region, offset_t offset, page_t **pagep);
} vm_region_ops_t;

/** Structure containing an anonymous memory map. */
typedef struct vm_amap {
	refcount_t count;		/**< Count of regions referring to this object. */
	mutex_t lock;			/**< Lock to protect map. */

	size_t curr_size;		/**< Number of pages currently contained in object. */
	size_t max_size;		/**< Maximum number of pages in object. */
	page_t **pages;			/**< Array of pages currently in object. */
	uint16_t *rref;			/**< Region reference count array. */
} vm_amap_t;

/** Structure representing a region in an address space. */
typedef struct vm_region {
	list_t header;			/**< Link to the region list. */
	list_t free_link;		/**< Link to free region lists. */
	avl_tree_node_t tree_link;	/**< Link to allocated region tree. */

	struct vm_aspace *as;		/**< Address space that the region belongs to. */
	ptr_t start;			/**< Base address of the region. */
	size_t size;			/**< Size of the region. */
	uint32_t access;		/**< Access flags for the region. */
	uint32_t flags;			/**< Region behaviour flags. */

	/** Allocation state of the region. */
	enum {
		VM_REGION_FREE,		/**< Region is free. */
		VM_REGION_ALLOCATED,	/**< Region is in use. */
		VM_REGION_RESERVED,	/**< Region is reserved, must not be allocated. */
	} state;

	object_handle_t *handle;	/**< Handle to object that this region is mapping. */
	offset_t obj_offset;		/**< Offset into the object. */
	vm_amap_t *amap;		/**< Anonymous map. */
	offset_t amap_offset;		/**< Offset into the anonymous map. */
	vm_region_ops_t *ops;		/**< Operations provided by the object. */
	void *private;			/**< Private data for the object type. */

	/** Kernel locking state. */
	size_t locked;			/**< Number of calls to vm_lock_page() on the region. */
	condvar_t waiters;		/**< Condition to wait for region to be unlocked on. */

	char *name;			/**< Name of the region (can be NULL). */
} vm_region_t;

/** Structure containing a virtual address space. */
typedef struct vm_aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */

	/** Address lookup stuff. */
	vm_region_t *find_cache;	/**< Cached pointer to last region searched for. */
	avl_tree_t tree;		/**< Tree of mapped regions for address lookups. */

	/** Underlying MMU context for address space. */
	struct mmu_context *mmu;

	/** Free region allocation. */
	list_t free[VM_FREELISTS];	/**< Power of 2 free lists. */
	ptr_t free_map;			/**< Bitmap of free lists with regions. */

	/** Sorted list of all (including unused) regions. */
	list_t regions;
} vm_aspace_t;

/** Page fault reason codes. */
#define VM_FAULT_UNMAPPED	1	/**< Fault on an unmapped virtual address. */
#define VM_FAULT_ACCESS		2	/**< Fault caused by an access violation. */

extern status_t vm_lock_page(vm_aspace_t *as, ptr_t addr, uint32_t access,
	phys_ptr_t *physp);
extern void vm_unlock_page(vm_aspace_t *as, ptr_t addr);

extern void vm_fault(struct intr_frame *frame, ptr_t addr, int reason,
	uint32_t access);

extern status_t vm_map(vm_aspace_t *as, ptr_t *addrp, size_t size, unsigned spec,
	uint32_t access, uint32_t flags, object_handle_t *handle,
	offset_t offset, const char *name);
extern status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);
extern status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(vm_aspace_t *parent);
extern vm_aspace_t *vm_aspace_clone(vm_aspace_t *parent);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

#endif /* __MM_VM_H */
