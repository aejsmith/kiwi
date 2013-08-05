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
 * @brief		AMD64 interrupt handling functions.
 */

#include <arch/memory.h>

#include <x86/cpu.h>
#include <x86/fpu.h>
#include <x86/intr.h>

#include <device/irq.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/safe.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/signal.h>
#include <proc/thread.h>

#include <kdb.h>
#include <kernel.h>
#include <setjmp.h>
#include <status.h>

extern void kdb_db_handler(intr_frame_t *frame);
extern void intr_handler(intr_frame_t *frame);

/** Array of interrupt handling routines. */
intr_handler_t intr_table[IDT_ENTRY_COUNT];

/** String names for CPU exceptions. */
static const char *except_strings[] = {
	"Divide Error", "Debug", "Non-Maskable Interrupt", "Breakpoint",
	"Overflow", "BOUND Range Exceeded", "Invalid Opcode",
	"Device Not Available", "Double Fault", "Coprocessor Segment Overrun",
	"Invalid TSS", "Segment Not Present", "Stack Fault",
	"General Protection Fault", "Page Fault", "Reserved",
	"FPU Error", "Alignment Check", "Machine Check",
	"SIMD Error", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved",
};

/** Unhandled interrupt function.
 * @param frame		Interrupt stack frame. */
static void unhandled_interrupt(intr_frame_t *frame) {
	if(atomic_get(&kdb_running) == 2) {
		kdb_except_handler("Unknown", frame);
	} else {
		fatal_etc(frame, "Received unknown interrupt %lu", frame->num);
	}
}

/** Hardware interrupt wrapper.
 * @param frame		Interrupt stack frame. */
static void hardware_interrupt(intr_frame_t *frame) {
	/* Hardware IRQs start at 32. */
	irq_handler(frame->num - 32);
}

/** Unhandled kernel-mode exception handler.
 * @param frame		Interrupt stack frame. */
static void kmode_except_handler(intr_frame_t *frame) {
	/* All unhandled kernel-mode exceptions are fatal. When in KDB, pass
	 * through to its exception handler. */
	if(atomic_get(&kdb_running) == 2) {
		kdb_except_handler(except_strings[frame->num], frame);
	} else {
		fatal_etc(frame, "Unhandled kernel-mode exception %lu (%s)",
			frame->num, except_strings[frame->num]);
	}
}

/** Generic exception handler.
 * @param frame		Interrupt stack frame. */
static void except_handler(intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		kprintf(LOG_DEBUG, "arch: sending SIGSEGV to thread %" PRId32 " (%s) "
			"due to exception %lu (%s) at %p\n", curr_thread->id,
			curr_thread->name, frame->num, except_strings[frame->num],
			frame->ip);
		kdb_enter(KDB_REASON_USER, frame);

		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGSEGV, &info, true);
	} else {
		kmode_except_handler(frame);
	}
}

/** Divide Error (#DE) fault handler.
 * @param frame		Interrupt stack frame. */
static void de_fault(intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		kprintf(LOG_DEBUG, "arch: sending SIGFPE to thread %" PRId32 " (%s) "
			"due to exception %lu (%s) at %p\n", curr_thread->id,
			curr_thread->name, frame->num, except_strings[frame->num],
			frame->ip);


		memset(&info, 0, sizeof(info));
		info.si_code = FPE_INTDIV;
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(frame);
	}
}

/** Handler for NMIs.
 * @param frame		Interrupt stack frame. */
static void nmi_handler(intr_frame_t *frame) {
	if(atomic_get(&kdb_running) > 0) {
		while(atomic_get(&kdb_running) > 0)
			arch_cpu_spin_hint();

		return;
	}

	fatal_etc(frame, "Received unexpected NMI");
}

/** Invalid Opcode (#UD) fault handler.
 * @param frame		Interrupt stack frame. */
static void ud_fault(intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		kprintf(LOG_DEBUG, "arch: sending SIGILL to thread %" PRId32 " (%s) "
			"due to exception %lu (%s) at %p\n", curr_thread->id,
			curr_thread->name, frame->num, except_strings[frame->num],
			frame->ip);

		memset(&info, 0, sizeof(info));
		info.si_code = ILL_ILLOPC;
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGILL, &info, true);
	} else {
		kmode_except_handler(frame);
	}
}

/** Handler for device-not-available (#NM) exceptions.
 * @param frame		Interrupt stack frame. */
static void nm_fault(intr_frame_t *frame) {
	if(frame->cs & 3) {
		/* We're coming from user-mode, this is a valid request for FPU
		 * usage. Enable the FPU. */
		x86_fpu_enable();

		/* If the thread has the ARCH_THREAD_HAVE_FPU flag set, we have
		 * used the FPU previously and so have a state to restore.
		 * Otherwise, initialize a new state. */
		if(curr_thread->arch.flags & ARCH_THREAD_HAVE_FPU) {
			x86_fpu_restore(curr_thread->arch.fpu);
		} else {
			x86_fpu_init();
			curr_thread->arch.flags |= ARCH_THREAD_HAVE_FPU;
		}

		if(++curr_thread->arch.fpu_count >= 5) {
			/* We're using the FPU frequently, set a flag which
			 * causes the FPU state to be loaded during a thread
			 * switch. */
			curr_thread->arch.flags |= ARCH_THREAD_FREQUENT_FPU;
		}
	} else {
		/* FPU usage is not allowed in kernel-mode. */
		kmode_except_handler(frame);
	}
}

/** Handler for double faults.
 * @param frame		Interrupt stack frame. */
static void double_fault(intr_frame_t *frame) {
	fatal_etc(frame, "Double fault", frame->ip);
	arch_cpu_halt();
}

/** Handler for page faults.
 * @param frame		Interrupt stack frame. */
static void page_fault(intr_frame_t *frame) {
	ptr_t addr;
	int reason;
	uint32_t access;
	status_t ret;

	/* We can't service a page fault while running KDB. */
	if(unlikely(atomic_get(&kdb_running) == 2)) {
		kdb_except_handler(except_strings[frame->num], frame);
		return;
	}

	addr = x86_read_cr2();
	reason = (frame->err_code & (1<<0)) ? VM_FAULT_PROTECTION
		: VM_FAULT_NOT_PRESENT;
	access = (cpu_features.xd && frame->err_code & (1<<4))
		? VM_PROT_EXECUTE
		: ((frame->err_code & (1<<1)) ? VM_PROT_WRITE : VM_PROT_READ);

	/* Check if a reserved bit fault. This is always fatal. */
	if(frame->err_code & (1<<3))
		fatal("Reserved bit page fault exception at %p", addr);;

	ret = vm_fault(frame, addr, reason, access);
	if(ret != STATUS_SUCCESS) {
		/* Handle faults in safe user memory access functions. */
		if(curr_thread && curr_thread->in_usermem && is_user_address((void *)addr)) {
			kprintf(LOG_DEBUG, "arch: thread %" PRId32 " (%s) faulted "
				"in usermem at %p (ip: %p)\n", curr_thread->id,
				curr_thread->name, addr, frame->ip);
			kdb_enter(KDB_REASON_USER, frame);
			longjmp(curr_thread->usermem_context, 1);
		}

		fatal_etc(frame, "Unhandled %s-mode page fault exception at %p (%d)",
			(frame->cs & 3) ? "user" : "kernel", addr, ret);
	}
}

/** FPU Floating-Point Error (#MF) fault handler.
 * @todo		Get FPU status and convert to correct signal code.
 * @param frame		Interrupt stack frame. */
static void mf_fault(intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(frame);
	}
}

/** SIMD Floating-Point (#XM) fault handler.
 * @todo		Get FPU status and convert to correct signal code.
 * @param frame		Interrupt stack frame. */
static void xm_fault(intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(frame);
	}
}

/** Interrupt handler.
 * @todo		Move this into entry.S, call the handler directly from
 *			the entry code.
 * @param frame		Interrupt frame. */
void intr_handler(intr_frame_t *frame) {
	bool user = frame->cs & 3;

	if(user) {
		/* Save the user-mode interrupt frame pointer, used by the
		 * signal frame setup/restore code. */
		curr_thread->arch.user_iframe = frame;
		thread_at_kernel_entry();
	}

	/* Call the handler. */
	intr_table[frame->num](frame);

	if(user) {
		thread_at_kernel_exit();

		/* We must clear the ARCH_THREAD_IFRAME_MODIFIED flag if it has
		 * been set. This is used in the SYSCALL handler below so that
		 * it knows whether to return via the IRET path, but as we're
		 * returning using IRET anyway it doesn't matter to us. */
		curr_thread->arch.flags &= ~ARCH_THREAD_IFRAME_MODIFIED;
	} else {
		/* Preempt if required. When returning to userspace, this is
		 * done by thread_at_kernel_exit(). */
		if(curr_cpu->should_preempt)
			thread_preempt();
	}
}

/** Initialize the interrupt handler table. */
__init_text void intr_init(void) {
	size_t i;

	/* Install default handlers. 0-31 are exceptions, 32-47 are IRQs, the
	 * rest should be pointed to the unhandled interrupt function */
	for(i = 0; i < 32; i++)
		intr_table[i] = except_handler;
	for(i = 32; i < 48; i++)
		intr_table[i] = hardware_interrupt;
	for(i = 48; i < ARRAY_SIZE(intr_table); i++)
		intr_table[i] = unhandled_interrupt;

	/* Set handlers for faults that require specific handling. */
	intr_table[X86_EXCEPT_DE]  = de_fault;
	intr_table[X86_EXCEPT_DB]  = kdb_db_handler;
	intr_table[X86_EXCEPT_NMI] = nmi_handler;
	intr_table[X86_EXCEPT_UD]  = ud_fault;
	intr_table[X86_EXCEPT_NM]  = nm_fault;
	intr_table[X86_EXCEPT_DF]  = double_fault;
	intr_table[X86_EXCEPT_PF]  = page_fault;
	intr_table[X86_EXCEPT_MF]  = mf_fault;
	intr_table[X86_EXCEPT_XM]  = xm_fault;
}
