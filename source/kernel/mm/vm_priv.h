/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		VM internal definitions.
 */

#ifndef __VM_PRIV_H
#define __VM_PRIV_H

#include <mm/vm.h>
#include <console.h>

#if CONFIG_VM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

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
	list_t header;			/**< Link to the region list. */
	list_t free_link;		/**< Link to free region lists. */
	avl_tree_node_t tree_link;	/**< Link to allocated region tree. */

	vm_aspace_t *as;		/**< Address space that the region belongs to. */
	ptr_t start;			/**< Base address of the region. */
	ptr_t end;			/**< End address of the region. */
	int flags;			/**< Flags for the region (0 if region is free). */

	object_handle_t *handle;	/**< Handle to object that this region is mapping. */
	offset_t obj_offset;		/**< Offset into the object. */
	vm_amap_t *amap;		/**< Anonymous map. */
	offset_t amap_offset;		/**< Offset into the anonymous map. */
} vm_region_t;

/** Region behaviour flags. */
#define VM_REGION_READ		VM_MAP_READ
#define VM_REGION_WRITE		VM_MAP_WRITE
#define VM_REGION_EXEC		VM_MAP_EXEC
#define VM_REGION_PRIVATE	VM_MAP_PRIVATE
#define VM_REGION_STACK		VM_MAP_STACK
#define VM_REGION_RESERVED	(1<<5)	/**< Region is reserved and should never be allocated. */

#endif /* __VM_PRIV_H */
