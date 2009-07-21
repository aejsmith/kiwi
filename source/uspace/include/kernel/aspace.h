/* Kiwi address space functions
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
 * @brief		Address space functions.
 */

#ifndef __KERNEL_ASPACE_H
#define __KERNEL_ASPACE_H

#include <kernel/types.h>

#define __need_size_t
#include <stddef.h>

/** Address space mapping flags. */
#define ASPACE_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define ASPACE_MAP_WRITE	(1<<1)	/**< Mapping should be writable. */
#define ASPACE_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */
#define ASPACE_MAP_FIXED	(1<<3)	/**< Mapping should be placed at the exact location specified. */
#define ASPACE_MAP_PRIVATE	(1<<4)	/**< Mapping should never be shared between address spaces. */

extern int aspace_map_anon(void *start, size_t size, int flags, void **addrp);
extern int aspace_map_file(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp);
extern int aspace_unmap(void *start, size_t size);

#endif /* __KERNEL_ASPACE_H */
