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

#include <arch/memmap.h>

#include <types.h>

/*
 * x87 FPU functions.
 */

/** Initialize FPU state. */
static inline void fninit(void) {
	__asm__ volatile("fninit");
}

/** Save FPU state.
 * @param area		Area to save to. */
static inline void fnsave(char *area) {
	__asm__ volatile("fnsave %0" :: "m"(*area));
}

/** Restore FPU state.
 * @param area		Area to restore from. */
static inline void frstor(char *area) {
	__asm__ volatile("frstor %0" :: "m"(*area));
}

/** Save FPU state.
 * @param area		Area to save to. */
static inline void fxsave(char *area) {
	__asm__ volatile("fxsave %0" :: "m"(*area));
}

/** Restore FPU state.
 * @param area		Area to restore from. */
static inline void fxrstor(char *area) {
	__asm__ volatile("fxrstor %0" :: "m"(*area));
}

/*
 * Miscellaneous.
 */

/** Spin loop hint using the PAUSE instruction to be more friendly to certain
 * CPUs (Pentium 4 and Xeon, mostly) in terms of performance and energy
 * consumption - see PAUSE instruction in Intel Instruction Set Reference N-Z
 * manual for more information. */
static inline void spin_loop_hint(void) {
	__asm__ volatile("pause");
}

/** Invalidate a TLB entry.
 * @param addr		Address to invalidate. */
static inline void invlpg(ptr_t addr) {
	__asm__ volatile("invlpg (%0)" :: "r"(addr));
}

/** Set the GS register.
 * @param sel		Selector to set. */
static inline void set_gs(uint32_t sel) {
	__asm__ volatile("mov %%ax, %%gs" :: "a"(sel));
}

/** Execute the SWAPGS instruction. */
static inline void swapgs(void) {
	__asm__ volatile("swapgs");
}

#endif /* __ARCH_ASM_H */
