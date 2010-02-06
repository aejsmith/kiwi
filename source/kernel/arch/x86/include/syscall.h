/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		x86 system call stack frame structure.
 */

#ifndef __ARCH_SYSCALL_H
#define __ARCH_SYSCALL_H

#include <types.h>

/** x86 system call stack frame structure. */
typedef struct syscall_frame {
#ifdef __x86_64__
	/** Values that the system call dispatcher requires. */
	unative_t id;		/**< System call ID (RAX). */
	unative_t p1;		/**< First parameter (RDI). */
	unative_t p2;		/**< Second parameter (RSI). */
	unative_t p3;		/**< Third parameter (RDX). */
	unative_t p4;		/**< Fourth parameter (R10 - RCX used by SYSCALL). */
	unative_t p5;		/**< Fifth parameter (R8). */
	unative_t p6;		/**< Sixth parameter (R9). */

	/** AMD64-specific values - callee-save registers, etc. */
	unative_t rbp;		/**< RBP. */
	unative_t rbx;		/**< RBX. */
	unative_t r15;		/**< R15. */
	unative_t r14;		/**< R14. */
	unative_t r13;		/**< R13. */
	unative_t r12;		/**< R12. */
	unative_t rflags;	/**< RFLAGS (R11). */
	unative_t rip;		/**< RIP (RCX). */
	unative_t rsp;		/**< RSP (userspace stack pointer). */
#else
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t es;			/**< ES. */
	unative_t ds;			/**< DS. */
	unative_t p3;			/**< Third parameter (EDI). */
	unative_t p4;			/**< Fourth parameter (ESI). */
	unative_t p6;			/**< Sixth parameter (EBP). */
	unative_t ksp;			/**< ESP (kernel). */
	unative_t p5;			/**< Fifth parameter (EBX). */
	unative_t p1;			/**< First parameter (EDX). */
	unative_t p2;			/**< Second parameter (ECX). */
	unative_t id;			/**< System call ID (EAX). */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< EIP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< EFLAGS. */
	unative_t sp;			/**< ESP. */
	unative_t ss;			/**< SS. */
#endif
} __packed syscall_frame_t;

/** System call interrupt number. */
#ifndef __x86_64__
# define SYSCALL_INT_NO		0x80
#endif

#endif /* __ARCH_SYSCALL_H */
