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
 * @brief		x86-specific thread functions.
 */

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#include <types.h>

/** x86-specific thread structure. */
typedef struct thread_arch {
#ifdef __x86_64__
	/** SYSCALL/SYSRET data.
	 * @note		This must be at the start of the structure. The
	 *			kernel GS base address is pointed at this to
	 *			allow the kernel/user stack pointers to be
	 *			saved/restored. */
	ptr_t kernel_rsp;	/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;		/**< Saved RSP for returning to userspace. */
#endif
	ptr_t tls_base;		/**< TLS base address. */
} __packed thread_arch_t;

#endif /* __ARCH_THREAD_H */
