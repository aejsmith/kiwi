/* Kiwi AMD64 scheduler functions
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
 * @brief		AMD64 scheduler functions.
 */

#include <cpu/cpu.h>

#include <proc/sched.h>
#include <proc/thread.h>

/** AMD64-specific post-thread switch function. */
void sched_arch_post_switch(void) {
	/* Set the RSP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	curr_cpu->arch.tss.rsp0 = (ptr_t)curr_thread->kstack + KSTACK_SIZE;
}
