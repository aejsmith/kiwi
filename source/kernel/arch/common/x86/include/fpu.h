/* Kiwi x86 FPU functions
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
 * @brief		x86 FPU functions.
 */

#ifndef __ARCH_X86_FPU_H
#define __ARCH_X86_FPU_H

#include <types.h>

/** Initialize FPU state. */
static inline void fpu_state_init(void) {
	__asm__ volatile("fninit");
}

/** Save FPU state.
 * @param area		Area to save to. */
static inline void fpu_state_save(char *area) {
	__asm__ volatile("fxsave %0" :: "m"(*area));
}

/** Restore FPU state.
 * @param area		Area to restore from. */
static inline void fpu_state_restore(char *area) {
	__asm__ volatile("fxrstor %0" :: "m"(*area));
}

#endif /* __ARCH_X86_FPU_H */
