/* Kiwi virtual memory functions
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
 * @brief		Virtual memory functions.
 */

#ifndef __KERNEL_VM_H
#define __KERNEL_VM_H

#include <kernel/types.h>

/** Behaviour flags for vm_map_* functions. */
#define VM_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define VM_MAP_WRITE		(1<<1)	/**< Mapping should be writable. */
#define VM_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */
#define VM_MAP_PRIVATE		(1<<3)	/**< Modifications to the mapping should not be visible to other processes. */
#define VM_MAP_FIXED		(1<<4)	/**< Mapping should be placed at the exact location specified. */

extern int vm_map_anon(void *start, size_t size, int flags, void **addrp);
extern int vm_map_file(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp);
extern int vm_unmap(void *start, size_t size);

#endif /* __KERNEL_VM_H */
