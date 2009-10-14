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

#include <arch/x86/sysreg.h>

#include <cpu/fpu.h>

/** Save FPU state.
 *
 * Saves the FPU state to an FPU context structure.
 *
 * @param ctx		Context to restore.
 */
void fpu_context_save(fpu_context_t *ctx) {
	__asm__ volatile("fxsave (%0)" :: "r"(ctx->data));
}

/** Restore FPU state.
 *
 * Restores the FPU state from an FPU context structure.
 *
 * @param ctx		Context to restore.
 */
void fpu_context_restore(fpu_context_t *ctx) {
	__asm__ volatile("fxrstor (%0)" :: "r"(ctx->data));
}

/** Check whether the FPU is enabled.
 * @return		Whether the FPU is enabled. */
bool fpu_state(void) {
	return !(sysreg_cr0_read() & SYSREG_CR0_TS);
}

/** Enable FPU usage. */
void fpu_enable(void) {
	sysreg_cr0_write(sysreg_cr0_read() & ~SYSREG_CR0_TS);
}

/** Disable FPU usage. */
void fpu_disable(void) {
	sysreg_cr0_write(sysreg_cr0_read() | SYSREG_CR0_TS);
}

/** Reset the FPU state. */
void fpu_init(void) {
	__asm__ volatile("fninit");
}
