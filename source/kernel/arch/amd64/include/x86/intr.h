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
 * @brief		x86 interrupt handling definitions.
 */

#ifndef __X86_INTR_H
#define __X86_INTR_H

#include <arch/frame.h>

#include <x86/descriptor.h>

/** Definitions for hardware exception numbers. */
#define X86_EXCEPT_DE		0	/**< Divide Error. */
#define X86_EXCEPT_DB		1	/**< Debug. */
#define X86_EXCEPT_NMI		2	/**< Non-Maskable Interrupt. */
#define X86_EXCEPT_BP		3	/**< Breakpoint. */
#define X86_EXCEPT_OF		4	/**< Overflow. */
#define X86_EXCEPT_BR		5	/**< BOUND Range Exceeded. */
#define X86_EXCEPT_UD		6	/**< Invalid Opcode. */
#define X86_EXCEPT_NM		7	/**< Device Not Available. */
#define X86_EXCEPT_DF		8	/**< Double Fault. */
#define X86_EXCEPT_TS		10	/**< Invalid TSS. */
#define X86_EXCEPT_NP		11	/**< Segment Not Present. */
#define X86_EXCEPT_SS		12	/**< Stack Fault. */
#define X86_EXCEPT_GP		13	/**< General Protection Fault. */
#define X86_EXCEPT_PF		14	/**< Page Fault. */
#define X86_EXCEPT_MF		16	/**< x87 FPU Floating-Point Error. */
#define X86_EXCEPT_AC		17	/**< Alignment Check. */
#define X86_EXCEPT_MC		18	/**< Machine Check. */
#define X86_EXCEPT_XM		19	/**< SIMD Floating-Point. */

/** Interrupt handler function type. */
typedef void (*intr_handler_t)(intr_frame_t *frame);

extern intr_handler_t intr_table[IDT_ENTRY_COUNT];

extern void intr_init(void);

#endif /* __X86_INTR_H */
