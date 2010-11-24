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
 * @brief		AMD64 thread functions.
 */

#include <arch/memory.h>

#include <cpu/cpu.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <status.h>

/** AMD64-specific post-thread switch function. */
void thread_arch_post_switch(thread_t *thread) {
	/* Set the RSP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	thread->cpu->arch.tss.rsp0 = (ptr_t)thread->kstack + KSTACK_SIZE;

	/* Store the address of the thread's architecture data in the
	 * KERNEL_GS_BASE MSR for the SYSCALL handler to use. */
        x86_write_msr(X86_MSR_K_GS_BASE, (ptr_t)&thread->arch);

	/* Set the FS base address. */
	x86_write_msr(X86_MSR_FS_BASE, thread->arch.tls_base);
}

/** Initialise AMD64-specific thread data.
 * @param thread	Thread to initialise.
 * @return		Always returns STATUS_SUCCESS. */
status_t thread_arch_init(thread_t *thread) {
	thread->arch.kernel_rsp = (ptr_t)thread->kstack + KSTACK_SIZE;
	thread->arch.user_rsp = 0;
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

	/* The AMD64 ABI uses the FS segment register to access the TLS data.
	 * Save the address to be written to the FS base upon each thread
	 * switch. */
	thread->arch.tls_base = (ptr_t)addr;
	if(thread == curr_thread) {
		x86_write_msr(X86_MSR_FS_BASE, thread->arch.tls_base);
	}

	return STATUS_SUCCESS;
}

/** Clean up AMD64-specific thread data.
 * @param thread	Thread to clean up. */
void thread_arch_destroy(thread_t *thread) {
	/* Nothing happens. */
}
