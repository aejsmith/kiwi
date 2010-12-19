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
 * @brief		x86 interrupt functions.
 */

#include <arch/x86/cpu.h>
#include <arch/memory.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/signal.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>

extern atomic_t cpu_pause_wait;
extern atomic_t cpu_halting_all;
extern void kdbg_db_handler(unative_t num, intr_frame_t *frame);
extern void intr_handler(intr_frame_t *frame);

/** Array of interrupt handling routines. */
intr_handler_t intr_handlers[IDT_ENTRY_COUNT];

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
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void unhandled_interrupt(unative_t num, intr_frame_t *frame) {
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, "Unknown", frame);
	} else {
		_fatal(frame, "Received unknown interrupt %" PRIun, num);
	}
}

/** Kernel-mode exception handler.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void kmode_except_handler(unative_t num, intr_frame_t *frame) {
	/* All unhandled kernel-mode exceptions are fatal. When in KDBG, pass
	 * through to its exception handler. */
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, except_strings[num], frame);
	} else {
		_fatal(frame, "Unhandled kernel-mode exception %" PRIun " (%s)", num, except_strings[num]);
	}
}

/** Generic exception handler.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void except_handler(unative_t num, intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGSEGV, &info, true);
	} else {
		kmode_except_handler(num, frame);
	}
}

/** Divide Error (#DE) fault handler.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void de_fault(unative_t num, intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_code = FPE_INTDIV;
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(num, frame);
	}
}

/** Handler for NMIs.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void nmi_handler(unative_t num, intr_frame_t *frame) {
	if(atomic_get(&cpu_halting_all)) {
		cpu_halt();
	} else if(atomic_get(&cpu_pause_wait)) {
		/* A CPU is in KDBG, assume that it wants us to pause
		 * execution until it has finished. */
		while(atomic_get(&cpu_pause_wait));
	} else {
		_fatal(frame, "Received unexpected NMI");
	}
}

/** Invalid Opcode (#UD) fault handler.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void ud_fault(unative_t num, intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_code = ILL_ILLOPC;
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGILL, &info, true);
	} else {
		kmode_except_handler(num, frame);
	}
}

/** Handler for device-not-available (#NM) exceptions.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void nm_fault(unative_t num, intr_frame_t *frame) {
	if(frame->cs & 3) {
		fpu_request();
	} else {
		kmode_except_handler(num, frame);
	}
}

/** Handler for double faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void double_fault(unative_t num, intr_frame_t *frame) {
#ifndef __x86_64__
	/* Copy in the state from before the fault into the frame. */
	frame->gs = curr_cpu->arch.tss.gs;
	frame->fs = curr_cpu->arch.tss.fs;
	frame->es = curr_cpu->arch.tss.es;
	frame->ds = curr_cpu->arch.tss.ds;
	frame->di = curr_cpu->arch.tss.edi;
	frame->si = curr_cpu->arch.tss.esi;
	frame->bp = curr_cpu->arch.tss.ebp;
	frame->bx = curr_cpu->arch.tss.ebx;
	frame->dx = curr_cpu->arch.tss.edx;
	frame->cx = curr_cpu->arch.tss.ecx;
	frame->ax = curr_cpu->arch.tss.eax;
	frame->ip = curr_cpu->arch.tss.eip;
	frame->cs = curr_cpu->arch.tss.cs;
	frame->flags = curr_cpu->arch.tss.eflags;
	frame->sp = curr_cpu->arch.tss.esp;
	frame->ss = curr_cpu->arch.tss.ss;
#endif
	_fatal(frame, "Double Fault", frame->ip);
	cpu_halt();
}

/** Handler for page faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void page_fault(unative_t num, intr_frame_t *frame) {
	int reason = (frame->err_code & (1<<0)) ? VM_FAULT_PROTECTION : VM_FAULT_NOTPRESENT;
	int access = (frame->err_code & (1<<1)) ? VM_FAULT_WRITE : VM_FAULT_READ;
	ptr_t addr = x86_read_cr2();
	siginfo_t info;
	int ret;

	/* We can't service a page fault while running KDBG. */
	if(unlikely(atomic_get(&kdbg_running) == 2)) {
		kdbg_except_handler(num, except_strings[num], frame);
		return;
	}
#if CONFIG_X86_NX
	/* Check if the fault was caused by instruction execution. */
	if(cpu_features.xd && frame->err_code & (1<<4)) {
		access = VM_FAULT_EXEC;
	}
#endif
	/* Check if a reserved bit fault. This is always fatal. */
	if(frame->err_code & (1<<3)) {
		fatal("Reserved bit PF exception (%p) (0x%x)", addr, frame->err_code);
	}

	/* Try the virtual memory manager if the fault occurred at a userspace
	 * address. */
	if(addr < (USER_MEMORY_BASE + USER_MEMORY_SIZE)) {
		ret = vm_fault(addr, reason, access);
		if(ret == VM_FAULT_SUCCESS) {
			return;
		} else if(curr_thread->in_usermem) {
			kprintf(LOG_DEBUG, "arch: pagefault in usermem at %p (ip: %p)\n", addr, frame->ip);
			kdbg_enter(KDBG_ENTRY_USER, frame);
			context_restore_frame(&curr_thread->usermem_context, frame);
			return;
		}
	} else {
		/* This is an access to kernel memory, which should be reported
		 * to userspace as accessing non-existant memory. */
		ret = VM_FAULT_NOREGION;
	}

	/* Nothing could handle this fault. If it happened in the kernel,
	 * die, otherwise just kill the process. */
	if(frame->err_code & (1<<2)) {
		kprintf(LOG_DEBUG, "arch: unhandled pagefault in thread %" PRId32 " of process %" PRId32 " (%p)\n",
		        curr_thread->id, curr_proc->id, addr);
		kprintf(LOG_DEBUG, "arch:  %s | %s%s%s\n",
		        (frame->err_code & (1<<0)) ? "protection" : "not-present",
		        (frame->err_code & (1<<1)) ? "write" : "read",
		        (frame->err_code & (1<<3)) ? " | reserved-bit" : "",
		        (frame->err_code & (1<<4)) ? " | execute" : "");
		kdbg_enter(KDBG_ENTRY_USER, frame);

		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;

		/* Pick the signal number and code. */
		switch(ret) {
		case VM_FAULT_NOREGION:
			info.si_signo = SIGSEGV;
			info.si_code = SEGV_MAPERR;
			break;
		case VM_FAULT_ACCESS:
			info.si_signo = SIGSEGV;
			info.si_code = SEGV_ACCERR;
			break;
		case VM_FAULT_OOM:
			info.si_signo = SIGBUS;
			info.si_code = BUS_ADRERR;
			break;
		default:
			info.si_signo = SIGSEGV;
			break;
		}

		signal_send(curr_thread, info.si_signo, &info, true);
	} else {
		_fatal(frame, "Unhandled kernel-mode pagefault exception (%p)\n"
		              "%s | %s%s%s", addr,
		              (frame->err_code & (1<<0)) ? "Protection" : "Not-present",
		              (frame->err_code & (1<<1)) ? "Write" : "Read",
		              (frame->err_code & (1<<3)) ? " | Reserved-bit" : "",
		              (frame->err_code & (1<<4)) ? " | Execute" : "");
	}
}

/** FPU Floating-Point Error (#MF) fault handler.
 * @todo		Get FPU status and convert to correct signal code.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void mf_fault(unative_t num, intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(num, frame);
	}
}

/** SIMD Floating-Point (#XM) fault handler.
 * @todo		Get FPU status and convert to correct signal code.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame. */
static void xm_fault(unative_t num, intr_frame_t *frame) {
	siginfo_t info;

	if(frame->cs & 3) {
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		signal_send(curr_thread, SIGFPE, &info, true);
	} else {
		kmode_except_handler(num, frame);
	}
}

/** Register an interrupt handler.
 *
 * Registers a handler to be called upon receipt of a certain interrupt. If
 * a handler exists for the interrupt then it will be overwritten.
 *
 * @param num		Interrupt number.
 * @param handler	Function pointer to handler routine.
 */
void intr_register(unative_t num, intr_handler_t handler) {
	assert(num < IDT_ENTRY_COUNT);
	intr_handlers[num] = handler;
}

/** Remove an interrupt handler.
 * @param num		Interrupt number. */
void intr_remove(unative_t num) {
	assert(num < IDT_ENTRY_COUNT);
	intr_handlers[num] = NULL;
}

/** Interrupt handler.
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
	intr_handlers[frame->int_no](frame->int_no, frame);

	if(user) {
		thread_at_kernel_exit();
#ifdef __x86_64__
		/* We must clear the THREAD_ARCH_IFRAME_MODIFIED flag if it has
		 * been set. This is used in the SYSCALL handler below so that
		 * it knows whether to return via the IRET path, but as we're
		 * returning using IRET anyway it doesn't matter to us. */
		curr_thread->arch.flags &= ~THREAD_ARCH_IFRAME_MODIFIED;
#endif
	} else {
		/* Preempt if required. When returning to userspace, this is
		 * done by thread_at_kernel_exit(). */
		if(curr_cpu->should_preempt) {
			sched_preempt();
		}
	}
}

/** Initialise the interrupt handler table. */
void intr_init(void) {
	size_t i;

	/* Install default handlers. 0-31 are exceptions, 32-47 are IRQs, the
	 * rest should be pointed to the unhandled interrupt function */
	for(i = 0; i < 32; i++) {
		intr_handlers[i] = except_handler;
	}
	for(i = 32; i < 48; i++) {
		intr_handlers[i] = irq_handler;
	}
	for(i = 48; i < ARRAYSZ(intr_handlers); i++) {
		intr_handlers[i] = unhandled_interrupt;
	}

	/* Set handlers for faults that require specific handling. */
	intr_handlers[X86_EXCEPT_DE]  = de_fault;
	intr_handlers[X86_EXCEPT_DB]  = kdbg_db_handler;
	intr_handlers[X86_EXCEPT_NMI] = nmi_handler;
	intr_handlers[X86_EXCEPT_UD]  = ud_fault;
	intr_handlers[X86_EXCEPT_NM]  = nm_fault;
	intr_handlers[X86_EXCEPT_DF]  = double_fault;
	intr_handlers[X86_EXCEPT_PF]  = page_fault;
	intr_handlers[X86_EXCEPT_MF]  = mf_fault;
	intr_handlers[X86_EXCEPT_XM]  = xm_fault;

	/* Set up the arch-independent IRQ subsystem. */
	irq_init();
}
