/* Kiwi IA32-specific thread structure
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
 * @brief		IA32-specific thread structure.
 */

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#include <types.h>

struct thread;

/** IA32-specific thread structure. */
typedef struct thread_arch {
	/* Nothing happens. */
} __packed thread_arch_t;

extern void thread_arch_post_switch(struct thread *thread);

extern int thread_arch_init(struct thread *thread);
extern void thread_arch_destroy(struct thread *thread);

#endif /* __ARCH_THREAD_H */
