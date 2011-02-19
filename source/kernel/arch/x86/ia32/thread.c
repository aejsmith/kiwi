/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		IA32 thread functions.
 */

#include <arch/memory.h>

#include <cpu/cpu.h>

#include <mm/safe.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <status.h>

extern void ia32_enter_userspace(ptr_t entry, ptr_t sp) __noreturn;

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

/** Clean up IA32-specific thread data.
 * @param thread	Thread to clean up. */
void thread_arch_destroy(thread_t *thread) {
	/* Nothing happens. */
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

/** Enter userspace in the current thread.
 * @param entry		Entry function.
 * @param stack		Stack pointer.
 * @param arg		Argument to function. */
void thread_arch_enter_userspace(ptr_t entry, ptr_t stack, ptr_t arg) {
	/* Write the argument to the function to the stack. */
	stack -= sizeof(unative_t);
	if(memcpy_to_user((void *)stack, &arg, sizeof(arg)) != STATUS_SUCCESS) {
		thread_exit();
	}

	/* Write a 0 return address for the entry function. */
	stack -= sizeof(unative_t);
	if(memset_user((void *)stack, 0, sizeof(unative_t)) != STATUS_SUCCESS) {
		thread_exit();
	}

	ia32_enter_userspace(entry, stack);
}
