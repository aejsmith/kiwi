/* Kiwi AMD64-specific thread functions
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
 * @brief		AMD64-specific thread functions.
 */

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#include <types.h>

struct thread;

/** AMD64-specific thread structure. */
typedef struct thread_arch {
	/** SYSCALL/SYSRET data. This must be at the start of the structure. */
	ptr_t kernel_rsp;	/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;		/**< Saved RSP for returning to userspace. */
} __packed thread_arch_t;

extern void thread_arch_post_switch(struct thread *thread);

extern int thread_arch_init(struct thread *thread);
extern void thread_arch_destroy(struct thread *thread);

extern void thread_arch_enter_userspace(ptr_t entry, ptr_t stack, ptr_t arg);

#endif /* __ARCH_THREAD_H */
