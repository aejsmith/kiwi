/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Virtual memory manager.
 */

#ifndef __MM_VM_H
#define __MM_VM_H

#include <cpu/cpu.h>
#include <kernel/vm.h>
#include <lib/utility.h>
#include <mm/mmu.h>
#include <sync/mutex.h>
#include <object.h>

struct vm_region;

/** Number of free lists to use. */
#define VM_FREELISTS		(((int)BITS(ptr_t)) - PAGE_WIDTH)

/** Structure containing a virtual address space. */
typedef struct vm_aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */
	int flags;			/**< Flags for the address space. */

	/** Address lookup stuff. */
	struct vm_region *find_cache;	/**< Cached pointer to last region searched for. */
	avl_tree_t tree;		/**< Tree of mapped regions for address lookups. */

	/** Underlying MMU context for address space. */
	mmu_context_t *mmu;

	/** Free region allocation. */
	list_t free[VM_FREELISTS];	/**< Power of 2 free lists. */
	ptr_t free_map;			/**< Bitmap of free lists with regions. */

	/** Sorted list of all (including unused) regions. */
	list_t regions;
} vm_aspace_t;

/** Macro that expands to a pointer to the current address space. */
#define curr_aspace		(curr_cpu->aspace)

/** Address space flags. */
#define VM_ASPACE_MLOCK		(1<<0)	/**< All pages on allocated regions always mapped in. */

/** Page fault reason codes. */
#define VM_FAULT_NOTPRESENT	1	/**< Fault caused by a not present page. */
#define VM_FAULT_PROTECTION	2	/**< Fault caused by a protection violation. */

/** Page fault access codes. */
#define VM_FAULT_READ		VM_MAP_READ
#define VM_FAULT_WRITE		VM_MAP_WRITE
#define VM_FAULT_EXEC		VM_MAP_EXEC

/** Page fault status codes. */
#define VM_FAULT_SUCCESS	0	/**< Fault handled successfully. */
#define VM_FAULT_FAILURE	1	/**< Other failure. */
#define VM_FAULT_NOREGION	2	/**< Address not in valid region (SEGV_MAPERR). */
#define VM_FAULT_ACCESS		3	/**< Access denied to region (SEGV_ACCERR). */
#define VM_FAULT_OOM		4	/**< Out of memory (BUS_ADRERR). */

extern vm_aspace_t *kernel_aspace;

extern int vm_fault(ptr_t addr, int reason, int access);

extern status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);
extern status_t vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags,
                       object_handle_t *handle, offset_t offset, ptr_t *addrp);
extern status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(void);
extern vm_aspace_t *vm_aspace_clone(vm_aspace_t *orig);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

#endif /* __MM_VM_H */
