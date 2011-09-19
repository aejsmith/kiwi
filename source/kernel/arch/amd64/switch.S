/*
 * Copyright (C) 2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 context switch function.
 */

#include <x86/asm.h>

/** Perform a thread context switch.
 * @param %rdi		New stack pointer.
 * @param %rsi		Location to store old stack pointer at. */
FUNCTION_START(amd64_context_switch)
	/* Just need to save callee-save registers: RBP, RBX, R12-15. */
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	/* Save the current stack pointer. */
	movq	%rsp, (%rsi)
FUNCTION_START(amd64_context_restore)
	/* Set stack pointer. */
	movq	%rdi, %rsp

	/* Restore callee-save registers. */
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp

	/* Stack pointer points at return address, just ret. */
	ret
FUNCTION_END(amd64_context_switch)
FUNCTION_END(amd64_context_restore)