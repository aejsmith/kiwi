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
 * @brief		x86 stack definitions/functions.
 */

#ifndef __ARCH_STACK_H
#define __ARCH_STACK_H

/** Stack size definitions. */
#define KSTACK_SIZE	0x2000		/**< Kernel stack size (8KB). */
#define USTACK_SIZE	0x200000	/**< Userspace stack size (2MB). */
#ifdef __x86_64__
# define STACK_DELTA	8		/**< Stack delta. */
#else
# define STACK_DELTA	4		/**< Stack delta. */
#endif

#endif /* __ARCH_STACK_H */
