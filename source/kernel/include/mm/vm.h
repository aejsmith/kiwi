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

#include <sync/mutex.h>

#include <cpu.h>
#include <object.h>

struct intr_frame;
struct mmu_context;
struct vm_region;

/** Number of free lists to use. */
#define VM_FREELISTS		(((unsigned)BITS(ptr_t)) - PAGE_WIDTH)

/** Maximum length of a region name. */
#define REGION_NAME_MAX		32

/** Structure containing a virtual address space. */
typedef struct vm_aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */

	/** Address lookup stuff. */
	struct vm_region *find_cache;	/**< Cached pointer to last region searched for. */
	avl_tree_t tree;		/**< Tree of mapped regions for address lookups. */

	/** Underlying MMU context for address space. */
	struct mmu_context *mmu;

	/** Free region allocation. */
	list_t free[VM_FREELISTS];	/**< Power of 2 free lists. */
	ptr_t free_map;			/**< Bitmap of free lists with regions. */

	/** Sorted list of all (including unused) regions. */
	list_t regions;
} vm_aspace_t;

/** Macro that expands to a pointer to the current address space. */
#define curr_aspace		(curr_cpu->aspace)

/** Page fault reason codes. */
enum {
	VM_FAULT_NOT_PRESENT,		/**< Fault caused by a not present page. */
	VM_FAULT_PROTECTION,		/**< Fault caused by a protection violation. */
};

extern status_t vm_fault(struct intr_frame *frame, ptr_t addr, int reason,
	uint32_t access);

extern status_t vm_map(vm_aspace_t *as, ptr_t *addrp, size_t size, unsigned spec,
	uint32_t protection, uint32_t flags, object_handle_t *handle,
	offset_t offset, const char *name);
extern status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size);
extern status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size);

extern void vm_aspace_switch(vm_aspace_t *as);
extern vm_aspace_t *vm_aspace_create(vm_aspace_t *parent);
extern vm_aspace_t *vm_aspace_clone(vm_aspace_t *parent);
extern void vm_aspace_destroy(vm_aspace_t *as);

extern void vm_init(void);

#endif /* __MM_VM_H */
