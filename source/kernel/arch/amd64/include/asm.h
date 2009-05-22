/* Kiwi AMD64 miscellaneous ASM functions
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		AMD64 miscellaneous ASM functions.
 */

#ifndef __ARCH_ASM_H
#define __ARCH_ASM_H

#include <types.h>

/** Spin loop hint using the PAUSE instruction to be more friendly to certain
 * CPUs (Pentium 4 and Xeon, mostly) in terms of performance and energy
 * consumption - see PAUSE instruction in Intel Instruction Set Reference N-Z
 * manual for more information. */
static inline void spin_loop_hint(void) {
	__asm__ volatile("pause");
}

#endif /* __ARCH_ASM_H */
