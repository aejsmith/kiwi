/*
 * Copyright (C) 2011-2014 Alex Smith
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

#ifndef __X86_INTERRUPT_H
#define __X86_INTERRUPT_H

#include <arch/frame.h>

#include <x86/descriptor.h>

/** Definitions for hardware exception numbers. */
#define X86_EXCEPTION_DE	0	/**< Divide Error. */
#define X86_EXCEPTION_DB	1	/**< Debug. */
#define X86_EXCEPTION_NMI	2	/**< Non-Maskable Interrupt. */
#define X86_EXCEPTION_BP	3	/**< Breakpoint. */
#define X86_EXCEPTION_OF	4	/**< Overflow. */
#define X86_EXCEPTION_BR	5	/**< BOUND Range Exceeded. */
#define X86_EXCEPTION_UD	6	/**< Invalid Opcode. */
#define X86_EXCEPTION_NM	7	/**< Device Not Available. */
#define X86_EXCEPTION_DF	8	/**< Double Fault. */
#define X86_EXCEPTION_TS	10	/**< Invalid TSS. */
#define X86_EXCEPTION_NP	11	/**< Segment Not Present. */
#define X86_EXCEPTION_SS	12	/**< Stack Fault. */
#define X86_EXCEPTION_GP	13	/**< General Protection Fault. */
#define X86_EXCEPTION_PF	14	/**< Page Fault. */
#define X86_EXCEPTION_MF	16	/**< x87 FPU Floating-Point Error. */
#define X86_EXCEPTION_AC	17	/**< Alignment Check. */
#define X86_EXCEPTION_MC	18	/**< Machine Check. */
#define X86_EXCEPTION_XM	19	/**< SIMD Floating-Point. */

/** Interrupt handler function type. */
typedef void (*interrupt_handler_t)(frame_t *frame);

extern interrupt_handler_t interrupt_table[IDT_ENTRY_COUNT];

extern void interrupt_init(void);

#endif /* __X86_INTERRUPT_H */
