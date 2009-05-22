/* Kiwi AMD64 stack definitions/functions
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
 * @brief		AMD64 stack definitions/functions.
 */

#ifndef __ARCH_STACK_H
#define __ARCH_STACK_H

/** Stack size definitions. */
#define KSTACK_SIZE	0x1000		/**< Kernel stack size (4KB). */
#define USTACK_SIZE	0x400000	/**< Userspace stack size (4MB). */
#define STACK_DELTA	8		/**< Stack delta. */

#ifndef __ASM__

#include <types.h>

/** Get the current stack pointer.
 * @return		Current stack pointer. */
static inline ptr_t stack_get_pointer(void) {
	ptr_t ret;

	__asm__ volatile("movq %%rsp, %0" : "=r"(ret));
	return ret;
}

/** Get the base of the current stack.
 * @note		Assumes stack is aligned KSTACK_SIZE and is
 *			KSTACK_SIZE.
 * @return		Base of current stack. */
static inline unative_t *stack_get_base(void) {
	return (unative_t *)(stack_get_pointer() & ~(KSTACK_SIZE - 1));
}

#endif /* __ASM__ */
#endif /* __ARCH_STACK_H */
