/*
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
 * @brief		IA32 thread functions.
 */

#include <cpu/cpu.h>

#include <proc/sched.h>
#include <proc/thread.h>

extern void sys_thread_set_tls_addr(void *addr);

/** IA32-specific post-thread switch function. */
void thread_arch_post_switch(thread_t *thread) {
	/* Set the ESP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	thread->cpu->arch.tss.esp0 = (ptr_t)thread->kstack + KSTACK_SIZE;

	/* Update the segment base. It will be reloaded upon return to
	 * userspace. */
	gdt_set_base(SEGMENT_U_GS, thread->arch.tls_base);
}

/** Initialise IA32-specific thread data.
 * @param thread	Thread to initialise.
 * @return		Always returns 0. */
int thread_arch_init(thread_t *thread) {
	thread->arch.tls_base = 0;
	return 0;
}

/** Clean up IA32-specific thread data.
 * @param thread	Thread to clean up. */
void thread_arch_destroy(thread_t *thread) {
	/* Nothing happens. */
}

/** Set the current thread's TLS address.
 * @param addr		TLS base address. */
void sys_thread_set_tls_addr(void *addr) {
	/* The IA32 ABI uses the GS segment register to access the TLS data.
	 * Save the address to be set upon each context switch. */
	curr_thread->arch.tls_base = (ptr_t)addr;

	/* Update the segment base. It will be reloaded upon return to
	 * userspace. */
	gdt_set_base(SEGMENT_U_GS, (ptr_t)addr);
}
