/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 FPU functions.
 */

#include <x86/cpu.h>
#include <cpu/fpu.h>

/** Save FPU state.
 * @param ctx		Context to restore. */
void fpu_context_save(fpu_context_t *ctx) {
	__asm__ volatile("fxsave (%0)" :: "r"(ctx->data));
}

/** Restore FPU state.
 * @param ctx		Context to restore. */
void fpu_context_restore(fpu_context_t *ctx) {
	__asm__ volatile("fxrstor (%0)" :: "r"(ctx->data));
}

/** Check whether the FPU is enabled.
 * @return		Whether the FPU is enabled. */
bool fpu_state(void) {
	return !(x86_read_cr0() & X86_CR0_TS);
}

/** Enable FPU usage. */
void fpu_enable(void) {
	x86_write_cr0(x86_read_cr0() & ~X86_CR0_TS);
}

/** Disable FPU usage. */
void fpu_disable(void) {
	x86_write_cr0(x86_read_cr0() | X86_CR0_TS);
}

/** Reset the FPU state. */
void fpu_init(void) {
	__asm__ volatile("fninit");
}
