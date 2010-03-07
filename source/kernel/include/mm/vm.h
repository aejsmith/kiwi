/*
 * Copyright (C) 2009 Alex Smith
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
#include <sync/mutex.h>
#include <object.h>

struct device;
struct vfs_node;
struct vm_aspace;
struct vm_region;

/** Structure tracking a page being used within the VM system. */
typedef struct vm_page {
	list_t header;			/**< Link to page lists. */
	phys_ptr_t addr;		/**< Physical address of the page. */
	refcount_t count;		/**< Reference count of the page. */
	offset_t offset;		/**< Offset into the object it belongs to. */
	int flags;			/**< Flags specifying information about the page. */
} vm_page_t;

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

/** Flags for the vm_page_t structure. */
#define VM_PAGE_DIRTY		(1<<0)	/**< Page is currently dirty. */

/** Flags for the vm_region_t structure. */
#define VM_REGION_READ		(1<<0)	/**< Region is readable. */
#define VM_REGION_WRITE		(1<<1)	/**< Region is writable. */
#define VM_REGION_EXEC		(1<<2)	/**< Region is executable. */
#define VM_REGION_PRIVATE	(1<<3)	/**< Modifications to this region should not be visible to other processes. */
#define VM_REGION_RESERVED	(1<<4)	/**< Region is reserved and should never be allocated. */

/** Behaviour flags for vm_map_* functions.
 * @note		Flags that have a region equivalent are defined to the
 *			same value as the region flag. */
#define VM_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define VM_MAP_WRITE		(1<<1)	/**< Mapping should be writable. */
#define VM_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */
#define VM_MAP_PRIVATE		(1<<3)	/**< Modifications to the mapping should not be visible to other processes. */
#define VM_MAP_FIXED		(1<<4)	/**< Mapping should be placed at the exact location specified. */

/** Page fault reason codes. */
#define VM_FAULT_NOTPRESENT	1	/**< Fault caused by a not present page. */
#define VM_FAULT_PROTECTION	2	/**< Fault caused by a protection violation. */

/** Page fault access codes.
 * @note		Defined to the same values as the region protection
 *			flags. */
#define VM_FAULT_READ		(1<<0)	/**< Fault caused by a read. */
#define VM_FAULT_WRITE		(1<<1)	/**< Fault caused by a write. */
#define VM_FAULT_EXEC		(1<<2)	/**< Fault when trying to execute. */

extern vm_page_t *vm_page_copy(vm_page_t *page, int mmflag);
extern vm_page_t *vm_page_alloc(int pmflag);
extern void vm_page_free(vm_page_t *page);

extern bool vm_fault(ptr_t addr, int reason, int access);

extern int vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);
extern int vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags, object_handle_t *handle,
                  offset_t offset, ptr_t *addrp);
extern int vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(void);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

extern int kdbg_cmd_aspace(int argc, char **argv);

/** Structure containing arguments for sys_vm_map(). */
typedef struct vm_map_args {
	void *start;			/**< Address to map at (if not VM_MAP_FIXED). */
	size_t size;			/**< Size of area to map (multiple of page size). */
	int flags;			/**< Flags controlling the mapping. */
	handle_t handle;		/**< Handle for file/device to map. */
	offset_t offset;		/**< Offset in the file/device to map from. */
	void **addrp;			/**< Where to store address mapped to. */
} vm_map_args_t;

extern int sys_vm_map(vm_map_args_t *args);
extern int sys_vm_unmap(void *start, size_t size);

#endif /* __MM_VM_H */
