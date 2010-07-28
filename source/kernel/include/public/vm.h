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

#ifndef __KERNEL_VM_H
#define __KERNEL_VM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

/** Behaviour flags for vm_map_* functions.
 * @note		Flags that have a region equivalent are defined to the
 *			same value as the region flag. */
#define VM_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define VM_MAP_WRITE		(1<<1)	/**< Mapping should be writable. */
#define VM_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */
#define VM_MAP_PRIVATE		(1<<3)	/**< Modifications to the mapping should not be visible to other processes. */
#define VM_MAP_STACK		(1<<4)	/**< Mapping contains a stack and should have a guard page. */
#define VM_MAP_FIXED		(1<<5)	/**< Mapping should be placed at the exact location specified. */

extern status_t SYSCALL(vm_map)(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp);
extern status_t SYSCALL(vm_unmap)(void *start, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_VM_H */
