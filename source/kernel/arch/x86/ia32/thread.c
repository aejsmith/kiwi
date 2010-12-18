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
 * @brief		IA32 thread functions.
 */

#include <arch/memory.h>

#include <cpu/cpu.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <status.h>

/** IA32-specific post-thread switch function. */
void thread_arch_post_switch(thread_t *thread) {
	/* Store the current CPU pointer and then point the GS register to the
	 * new thread's architecture data. */
	thread->arch.cpu = thread->cpu;
	gdt_set_base(thread->cpu, SEGMENT_K_GS, (ptr_t)&thread->arch);
	__asm__ volatile("mov %0, %%gs" :: "r"(SEGMENT_K_GS));

	/* Set the ESP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	curr_cpu->arch.tss.esp0 = (ptr_t)thread->kstack + KSTACK_SIZE;

	/* Update the GS segment base. It will be reloaded upon return to
	 * userspace. */
	gdt_set_base(curr_cpu, SEGMENT_U_GS, thread->arch.tls_base);
}

/** Initialise IA32-specific thread data.
 * @param thread	Thread to initialise.
 * @return		Always returns STATUS_SUCCESS. */
status_t thread_arch_init(thread_t *thread) {
	thread->arch.tls_base = 0;
	return STATUS_SUCCESS;
}

/** Get the TLS address for a thread.
 * @param thread	Thread to get for.
 * @return		TLS address of thread. */
ptr_t thread_arch_tls_addr(thread_t *thread) {
	return thread->arch.tls_base;
}

/** Set the TLS address for a thread.
 * @param thread	Thread to set for.
 * @param addr		TLS address.
 * @return		Status code describing result of the operation. */
status_t thread_arch_set_tls_addr(thread_t *thread, ptr_t addr) {
	if(addr >= (USER_MEMORY_BASE + USER_MEMORY_SIZE)) {
		return STATUS_INVALID_ADDR;
	}

	/* The IA32 ABI uses the GS segment register to access the TLS data.
	 * Save the address to be set upon each context switch. */
	thread->arch.tls_base = (ptr_t)addr;
	if(thread == curr_thread) {
		/* Update the segment base. It will be reloaded upon return to
		 * userspace. */
		gdt_set_base(curr_cpu, SEGMENT_U_GS, (ptr_t)addr);
	}

	return STATUS_SUCCESS;
}

/** Clean up IA32-specific thread data.
 * @param thread	Thread to clean up. */
void thread_arch_destroy(thread_t *thread) {
	/* Nothing happens. */
}
