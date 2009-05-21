/* Kiwi x86 fault handling functions
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
 * @brief		x86 fault handling functions.
 */

#ifndef __ARCH_X86_FAULT_H
#define __ARCH_X86_FAULT_H

#include <types.h>

/** Definitions for fault numbers. */
#define FAULT_DIVIDE		0	/**< Divide Error. */
#define FAULT_DEBUG		1	/**< Debug. */
#define FAULT_NMI		2	/**< Non-Maskable Interrupt. */
#define FAULT_BREAKPOINT	3	/**< Breakpoint. */
#define FAULT_OVERFLOW		4	/**< Overflow. */
#define FAULT_BOUND		5	/**< BOUND Range Exceeded. */
#define FAULT_INVALID_OPCODE	6	/**< Invalid Opcode. */
#define FAULT_DEVICE_NOT_AVAIL	7	/**< Device Not Available. */
#define FAULT_DOUBLE		8	/**< Double Fault. */
#define FAULT_COPROC_OVERRUN	9	/**< Coprocessor Segment Overrun. */
#define FAULT_INVALID_TSS	10	/**< Invalid TSS. */
#define FAULT_SEGMENT_NOT_PRES	11	/**< Segment Not Present. */
#define FAULT_STACK		12	/**< Stack Fault. */
#define FAULT_GP		13	/**< General Protection Fault. */
#define FAULT_PAGE		14	/**< Page Fault. */
#define FAULT_FPU		16	/**< x87 FPU Floating-Point Error. */
#define FAULT_ALIGNMENT		17	/**< Alignment Check. */
#define FAULT_MCE		18	/**< Machine Check. */
#define FAULT_SIMD		19	/**< SIMD Floating-Point. */

extern bool fault_handler(unative_t num, intr_frame_t *frame);

#endif /* __ARCH_X86_FAULT_H */
