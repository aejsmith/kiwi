/*
 * Copyright (C) 2009-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 thread functions.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/fpu.h>

#include <lib/string.h>

#include <mm/aspace.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <cpu.h>
#include <status.h>

extern void amd64_context_switch(ptr_t new_rsp, ptr_t *old_rsp);
extern void amd64_context_restore(ptr_t new_rsp);

/** Initialize AMD64-specific thread data.
 * @param thread	Thread to initialize.
 * @param stack		Base of the kernel stack for the thread.
 * @param entry		Entry point for the thread. */
void arch_thread_init(thread_t *thread, void *stack, void (*entry)(void)) {
	unsigned long *sp;

	thread->arch.parent = thread;
	thread->arch.flags = 0;
	thread->arch.tls_base = 0;
	thread->arch.fpu_count = 0;

	/* Point the RSP for SYSCALL entry at the top of the stack. */
	thread->arch.kernel_rsp = (ptr_t)stack + KSTACK_SIZE;

	/* Initialize the kernel stack. First value is a fake return address to
	 * make the backtrace end correctly and maintain ABI alignment
	 * requirements: ((RSP - 8) % 16) == 0 on entry to a function. */
	sp = (unsigned long *)thread->arch.kernel_rsp;
	*--sp = 0;			/* Fake return address for backtrace. */
	*--sp = (ptr_t)entry;		/* RIP/Return address. */
	*--sp = 0;			/* RBP. */
	*--sp = 0;			/* RBX. */
	*--sp = 0;			/* R12. */
	*--sp = 0;			/* R13. */
	*--sp = 0;			/* R14. */
	*--sp = 0;			/* R15. */

	/* Save the stack pointer for arch_thread_switch(). */
	thread->arch.saved_rsp = (ptr_t)sp;
}

/** Clean up AMD64-specific thread data.
 * @param thread	Thread to clean up. */
void arch_thread_destroy(thread_t *thread) {
	/* Nothing happens. */
}

/** Switch to another thread.
 * @param thread	Thread to switch to.
 * @param prev		Thread that was previously running. */
void arch_thread_switch(thread_t *thread, thread_t *prev) {
	bool fpu_enabled;

	/* Save the current FPU state, if any. */
	fpu_enabled = x86_fpu_state();
	if(likely(prev)) {
		if(fpu_enabled) {
			x86_fpu_save(prev->arch.fpu);
		} else {
			prev->arch.fpu_count = 0;
		}
	}

	/* Store the current CPU pointer and then point the GS register to the
	 * new thread's architecture data. The load of curr_cpu will load from
	 * the previous thread's architecture data. */
	thread->arch.cpu = curr_cpu;
	x86_write_msr(X86_MSR_GS_BASE, (ptr_t)&thread->arch);

	/* Set the RSP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	curr_cpu->arch.tss.rsp0 = thread->arch.kernel_rsp;

	/* Set the FS base address to the TLS segment base. */
	x86_write_msr(X86_MSR_FS_BASE, thread->arch.tls_base);

	/* Handle the FPU state. */
	if(thread->arch.flags & ARCH_THREAD_FREQUENT_FPU) {
		/* The FPU is being frequently used by the new thread, load the
		 * new state immediately so that the thread doesn't have to
		 * incur a fault before it can use the FPU again. */
		if(!fpu_enabled)
			x86_fpu_enable();

		x86_fpu_restore(thread->arch.fpu);
	} else {
		/* Disable the FPU. We switch the FPU state on demand in the
		 * new thread, to remove the overhead of loading it now when
		 * it is not likely that the FPU will be needed by the thread. */
		if(fpu_enabled)
			x86_fpu_disable();
	}

	/* Switch to the new context. */
	if(likely(prev)) {
		amd64_context_switch(thread->arch.saved_rsp, &prev->arch.saved_rsp);
	} else {
		/* Initial thread switch, don't have a previous thread. */
		amd64_context_restore(thread->arch.saved_rsp);
	}
}

/** Get the TLS address for a thread.
 * @param thread	Thread to get for.
 * @return		TLS address of thread. */
ptr_t arch_thread_tls_addr(thread_t *thread) {
	return thread->arch.tls_base;
}

/** Set the TLS address for a thread.
 * @param thread	Thread to set for.
 * @param addr		TLS address.
 * @return		Status code describing result of the operation. */
status_t arch_thread_set_tls_addr(thread_t *thread, ptr_t addr) {
	if(addr > USER_END)
		return STATUS_INVALID_ADDR;

	/* The AMD64 ABI uses the FS segment register to access the TLS data.
	 * Save the address to be written to the FS base upon each thread
	 * switch. */
	thread->arch.tls_base = (ptr_t)addr;
	if(thread == curr_thread)
		x86_write_msr(X86_MSR_FS_BASE, thread->arch.tls_base);

	return STATUS_SUCCESS;
}

/** Clone a thread.
 * @param thread	Cloned thread.
 * @param parent	Parent thread.
 * @param frame		Frame to fill with a copy of the parent's current frame. */
void arch_thread_clone(thread_t *thread, thread_t *parent, intr_frame_t *frame) {
	thread->arch.flags = parent->arch.flags & ARCH_THREAD_HAVE_FPU;
	thread->arch.tls_base = parent->arch.tls_base;

	/* Clone the parent's FPU state. */
	if(parent == curr_thread && x86_fpu_state()) {
		/* FPU is currently enabled so the latest state may not have
		 * been saved. */
		x86_fpu_save(thread->arch.fpu);
	} else if(parent->flags & ARCH_THREAD_HAVE_FPU) {
		memcpy(thread->arch.fpu, parent->arch.fpu, sizeof(thread->arch.fpu));
	}

	/* Duplicate the user interrupt frame. This should be valid as we
	 * only get here via a system call. */
	memcpy(frame, parent->arch.user_iframe, sizeof(*frame));

	/* The new thread should return success from the system call. */
	frame->ax = STATUS_SUCCESS;
}

/** Prepare a frame to enter userspace.
 * @param frame		Frame to prepare.
 * @param entry		Entry function.
 * @param stack		Stack pointer.
 * @param arg1		First argument to function.
 * @param arg2		Second argument to function. */
void arch_thread_prepare_userspace(intr_frame_t *frame, ptr_t entry,
	ptr_t stack, ptr_t arg1, ptr_t arg2)
{
	/* Correctly align the stack pointer for ABI requirements. */
	stack -= sizeof(unsigned long);

	/* Clear out the frame to zero all GPRs. */
	memset(frame, 0, sizeof(*frame));

	frame->di = arg1;
	frame->si = arg2;
	frame->ip = entry;
	frame->cs = USER_CS | 0x3;
	frame->flags = X86_FLAGS_IF | X86_FLAGS_ALWAYS1;
	frame->sp = stack;
	frame->ss = USER_DS | 0x3;
}
