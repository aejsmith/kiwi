/* Kiwi IA32 interrupt functions/definitions
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
 * @brief		IA32 interrupt functions/definitions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

#include <arch/x86/intr.h>

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t es;			/**< ES. */
	unative_t ds;			/**< DS. */
	unative_t di;			/**< EDI. */
	unative_t si;			/**< ESI. */
	unative_t bp;			/**< EBP. */
	unative_t ksp;			/**< ESP (kernel). */
	unative_t bx;			/**< EBX. */
	unative_t dx;			/**< EDX. */
	unative_t cx;			/**< ECX. */
	unative_t ax;			/**< EAX. */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< EIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< EFLAGS. */
	unative_t sp;			/**< ESP. */
	unative_t ss;			/**< SS. */
} __packed intr_frame_t;

#endif /* __ARCH_INTR_H */
