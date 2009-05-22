/* Kiwi AMD64 interrupt functions/definitions
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
 * @brief		AMD64 interrupt functions/definitions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

#include <arch/x86/intr.h>

/** Structure defining an interrupt stack frame. */
typedef struct intr_frame {
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t r15;			/**< R15. */
	unative_t r14;			/**< R14. */
	unative_t r13;			/**< R13. */
	unative_t r12;			/**< R12. */
	unative_t r11;			/**< R11. */
	unative_t r10;			/**< R10. */
	unative_t r9;			/**< R9. */
	unative_t r8;			/**< R8. */
	unative_t bp;			/**< RBP. */
	unative_t si;			/**< RSI. */
	unative_t di;			/**< RDI. */
	unative_t dx;			/**< RDX. */
	unative_t cx;			/**< RCX. */
	unative_t bx;			/**< RBX. */
	unative_t ax;			/**< RAX. */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< RIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< RFLAGS. */
	unative_t sp;			/**< RSP. */
	unative_t ss;			/**< SS. */
} __packed intr_frame_t;

#endif /* __ARCH_INTR_H */
