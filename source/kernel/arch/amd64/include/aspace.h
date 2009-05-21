/* Kiwi x86 address space definitions
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
 * @brief		x86 address space definitions.
 */

#ifndef __ARCH_ASPACE_H
#define __ARCH_ASPACE_H

#include <arch/memmap.h>

/** Address space size definitions. */
#define ASPACE_BASE		USPACE_BASE	/**< Start of userspace memory. */
#define ASPACE_SIZE		USPACE_SIZE	/**< Size of userspace memory. */

#endif /* __ARCH_ASPACE_H */
