/* Kiwi x86 spinlock helper functions
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
 * @brief		x86 spinlock helper functions.
 *
 * See PAUSE instruction in Intel 64 and IA-32 Architectures Software
 * Developer's Manual, Volume 2B: Instruction Set Reference N-Z for more
 * information as to why this function is necessary.
 */

#ifndef __ARCH_X86_SPINLOCK_H
#define __ARCH_X86_SPINLOCK_H

/** Spinlock loop hint using the PAUSE instruction. */
static inline void spinlock_loop_hint(void) {
	__asm__ volatile("pause");
}

#endif /* __ARCH_X86_SPINLOCK_H */
