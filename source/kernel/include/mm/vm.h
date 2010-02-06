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

#include <types/avl.h>
#include <types/refcount.h>

struct device;
struct vfs_node;
struct vm_aspace;
struct vm_object;
struct vm_region;

/** Structure tracking a page being used within the VM system. */
typedef struct vm_page {
	list_t header;			/**< Link to page lists. */
	phys_ptr_t addr;		/**< Physical address of the page. */
	refcount_t count;		/**< Reference count of the page. */
	offset_t offset;		/**< Offset into the object it belongs to. */
	int flags;			/**< Flags specifying information about the page. */
} vm_page_t;

/** Structure defining operations for an object. */
typedef struct vm_object_ops {
	/** Increase the reference count of an object.
	 * @param obj		Object to increase count of.
	 * @param region	Region that the reference is for. */
	void (*get)(struct vm_object *obj, struct vm_region *region);

	/** Decrease the reference count of an object.
	 * @param obj		Object to decrease count of.
	 * @param region	Region being detached. */
	void (*release)(struct vm_object *obj, struct vm_region *region);

	/** Map part of an object.
	 * @note		This (optional) operation is called when
	 *			creating a region that maps part of the object.
	 *			This includes duplicating shared regions to
	 *			a new address space (in this case, the get
	 *			operation is called before this one).
	 * @param obj		Object being mapped.
	 * @param offset	Offset into the object that the region starts
	 *			at.
	 * @param size		Size of the region being created.
	 * @return		0 on success, negative error code on failure. */
	int (*map)(struct vm_object *obj, offset_t offset, size_t size);

	/** Unmap part of an object.
	 * @note		This (optional) operation is called when all or
	 *			part of a region mapping the object is being
	 *			unmapped. If an entire region is being unmapped
	 *			this function is called before the release
	 *			operation for the object. This function is
	 *			called after any mapped pages have been
	 *			released.
	 * @param obj		Object being unmapped.
	 * @param offset	Offset into object being unmapped.
	 * @param size		Size of area being unmapped. */
	void (*unmap)(struct vm_object *obj, offset_t offset, size_t size);

	/** Create a copy of an object.
	 * @note		This is used when duplicating private regions
	 *			to a new address space. The destination region
	 *			will have the same start/end addresses and
	 *			flags. This function must set the object
	 *			pointer and offset into the object.
	 * @note		This is only really here to allow all of the
	 *			copy-on-write handling code to be moved into
	 *			the anonymous object type - all private regions
	 *			use anonymous objects. It should not be
	 *			implemented by any other object type.
	 * @param src		Source region to copy.
	 * @param dest		Destination region to copy to.
	 * @return		0 on success, negative error code on failure. */
	int (*copy)(struct vm_region *src, struct vm_region *dest);

	/** Non-standard fault handling.
	 * @note		If this operation is specified, it is called
	 *			on all faults on regions using the object
	 *			rather than using the standard fault handling
	 *			mechanism. In this case, the page_get operation
	 *			is not needed.
	 * @note		When this is called, the main fault handler
	 *			will have verified that the fault is allowed by
	 *			the region's access flags.
	 * @param region	Region fault occurred in.
	 * @param addr		Virtual address of fault (rounded down to base
	 *			of page).
	 * @param reason	Reason for the fault.
	 * @param access	Type of access that caused the fault.
	 * @return		Fault status code. */
	int (*fault)(struct vm_region *region, ptr_t addr, int reason, int access);

	/** Get a page from the object.
	 * @note		This operation is not required if the fault
	 *			operation is specified. If the fault operation
	 *			is not specified, then it is required.
	 * @param obj		Object to get page from.
	 * @param offset	Offset to get page from (the offset into the
	 *			region the fault occurred at, plus the offset
	 *			of the region into the object).
	 * @param pagep		Where to store pointer to page structure.
	 * @return		0 on success, negative error code on failure. */
	int (*page_get)(struct vm_object *obj, offset_t offset, vm_page_t **pagep);

	/** Release a page from the object.
	 * @param obj		Object to release page in.
	 * @param offset	Offset of page in object.
	 * @param paddr		Physical address of page that was unmapped. */
	void (*page_release)(struct vm_object *obj, offset_t offset, phys_ptr_t paddr);
} vm_object_ops_t;

/** Structure defining an object that can be memory-mapped.
 * @note		This structure is intended to be embedded within
 *			another structure (e.g. vfs_node_t). It is essentially
 *			a handle provided to the VM system to map an object
 *			in.
 * @note		The object structure does not provide reference
 *			counting for objects - the reference/detach operations
 *			are provided to allow the object type to manage
 *			reference counting itself. */
typedef struct vm_object {
	vm_object_ops_t *ops;		/**< Operations for the object. */
} vm_object_t;

/** Structure defining a region in an address space. */
typedef struct vm_region {
	struct vm_aspace *as;		/**< Address space region belongs to. */
	ptr_t start;			/**< Base address of the region. */
	ptr_t end;			/**< Size of the region. */
	int flags;			/**< Flags for the region. */

	list_t object_link;		/**< Link for object to use. */
	vm_object_t *object;		/**< Object that this region is mapping. */
	offset_t offset;		/**< Offset into the object. */

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

/** Page fault access codes. */
#define VM_FAULT_READ		1	/**< Fault caused by a read. */
#define VM_FAULT_WRITE		2	/**< Fault caused by a write. */
#define VM_FAULT_EXEC		3	/**< Fault when trying to execute. */

/** Page fault status codes. */
#define VM_FAULT_HANDLED	1	/**< Fault was handled and execution can resume. */
#define VM_FAULT_UNHANDLED	2	/**< Fault could not be handled. */

/** Convert region flags to page map flags.
 * @param flags         Flags to convert.
 * @return              Page map flags. */
static inline int vm_region_flags_to_page(int flags) {
        int ret = 0;

        ret |= ((flags & VM_REGION_READ) ? PAGE_MAP_READ : 0);
        ret |= ((flags & VM_REGION_WRITE) ? PAGE_MAP_WRITE : 0);
       	ret |= ((flags & VM_REGION_EXEC) ? PAGE_MAP_EXEC : 0);
        return ret;
}

extern vm_page_t *vm_page_copy(vm_page_t *page, int mmflag);
extern vm_page_t *vm_page_alloc(int pmflag);
extern void vm_page_free(vm_page_t *page);

extern void vm_object_init(vm_object_t *object, vm_object_ops_t *ops);
extern void vm_object_destroy(vm_object_t *object);

extern int vm_fault(ptr_t addr, int reason, int access);

#if 0
# pragma mark Public interface.
#endif

extern int vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);
extern int vm_map_anon(vm_aspace_t *as, ptr_t start, size_t size, int flags, ptr_t *addrp);
extern int vm_map_file(vm_aspace_t *as, ptr_t start, size_t size, int flags, struct vfs_node *node,
                       offset_t offset, ptr_t *addrp);
extern int vm_map_device(vm_aspace_t *as, ptr_t start, size_t size, int flags, struct device *device,
                         offset_t offset, ptr_t *addrp);
extern int vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(void);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

extern int kdbg_cmd_aspace(int argc, char **argv);

#if 0
# pragma mark System calls.
#endif

/** Structure containing arguments for sys_vm_map_file()/sys_vm_map_device(). */
typedef struct vm_map_args {
	void *start;			/**< Address to map at (if not VM_MAP_FIXED). */
	size_t size;			/**< Size of area to map (multiple of page size). */
	int flags;			/**< Flags controlling the mapping. */
	handle_t handle;		/**< Handle for file/device to map. */
	offset_t offset;		/**< Offset in the file/device to map from. */
	void **addrp;			/**< Where to store address mapped to. */
} vm_map_args_t;

extern int sys_vm_map_anon(void *start, size_t size, int flags, void **addrp);
extern int sys_vm_map_file(vm_map_args_t *args);
extern int sys_vm_map_device(vm_map_args_t *args);
extern int sys_vm_unmap(void *start, size_t size);

#endif /* __MM_VM_H */
