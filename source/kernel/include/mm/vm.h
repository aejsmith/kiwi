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
#include <kernel/vm.h>
#include <lib/utility.h>
#include <mm/page.h>
#include <sync/mutex.h>
#include <object.h>

struct vm_region;

/** Number of free lists to use. */
#define VM_FREELISTS		(((int)BITS(ptr_t)) - PAGE_WIDTH)

/** Structure containing a virtual address space. */
typedef struct vm_aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */

	/** Address lookup stuff. */
	struct vm_region *find_cache;	/**< Cached pointer to last region searched for. */
	avl_tree_t tree;		/**< Tree of mapped regions for address lookups. */

	/** Underlying page map for address space. */
	page_map_t pmap;

	/** Free region allocation. */
	list_t free[VM_FREELISTS];	/**< Power of 2 free lists. */
	ptr_t free_map;			/**< Bitmap of free lists with regions. */

	/** Sorted list of all (including unused) regions. */
	list_t regions;
} vm_aspace_t;

/** Macro that expands to a pointer to the current address space. */
#define curr_aspace		(curr_cpu->aspace)

/** Page fault reason codes. */
#define VM_FAULT_NOTPRESENT	1	/**< Fault caused by a not present page. */
#define VM_FAULT_PROTECTION	2	/**< Fault caused by a protection violation. */

/** Page fault access codes. */
#define VM_FAULT_READ		VM_MAP_READ
#define VM_FAULT_WRITE		VM_MAP_WRITE
#define VM_FAULT_EXEC		VM_MAP_EXEC

extern bool vm_fault(ptr_t addr, int reason, int access);

extern status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);
extern status_t vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags,
                       object_handle_t *handle, offset_t offset, ptr_t *addrp);
extern status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(void);
extern vm_aspace_t *vm_aspace_clone(vm_aspace_t *orig);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

extern int kdbg_cmd_aspace(int argc, char **argv);

#endif /* __MM_VM_H */
