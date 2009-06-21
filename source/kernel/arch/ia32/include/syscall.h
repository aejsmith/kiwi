/* Kiwi IA32 system call frame structure
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
 * @brief		IA32 system call stack frame structure.
 *
 * IA32 uses an interrupt to perform system calls - the system call frame
 * structure has an identical structure to an interrupt frame structure,
 * but we define this structure with some different variable names so that
 * the system call handler can get the correct things out of the structure.
 *
 * @note		This must remain in sync with intr_frame_t.
 */

#ifndef __ARCH_SYSCALL_H
#define __ARCH_SYSCALL_H

#include <types.h>

/** IA32 system call stack frame structure. */
typedef struct syscall_frame {
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t es;			/**< ES. */
	unative_t ds;			/**< DS. */
	unative_t p1;			/**< First parameter (EDI). */
	unative_t p2;			/**< Second parameter (ESI). */
	unative_t bp;			/**< EBP. */
	unative_t ksp;			/**< ESP (kernel). */
	unative_t p5;			/**< Fifth parameter (EBX). */
	unative_t p3;			/**< Third parameter (EDX). */
	unative_t p4;			/**< Fourth parameter (ECX). */
	unative_t id;			/**< System call ID (EAX). */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< EIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< EFLAGS. */
	unative_t sp;			/**< ESP. */
	unative_t ss;			/**< SS. */
} __packed syscall_frame_t;

/** System call interrupt number. */
#define SYSCALL_INT_NO		0x80

#endif /* __ARCH_SYSCALL_H */
