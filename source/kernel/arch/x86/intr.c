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

#include <arch/cpu.h>
#include <arch/memory.h>
#include <arch/page.h>

#include <cpu/intr.h>

#include <lib/utility.h>

#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>

extern atomic_t cpu_pause_wait;
extern atomic_t cpu_halting_all;

extern bool kdbg_int1_handler(unative_t num, intr_frame_t *frame);
extern void intr_handler(intr_frame_t *frame);

/** Array of interrupt handling routines. */
static intr_handler_t intr_handlers[IDT_ENTRY_COUNT];

/** String names for CPU exceptions. */
static const char *fault_names[] = {
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

/** Handler for NMIs.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
static bool intr_handle_nmi(unative_t num, intr_frame_t *frame) {
	if(atomic_get(&cpu_halting_all)) {
		cpu_halt();
	} else if(atomic_get(&cpu_pause_wait)) {
		/* A CPU is in KDBG, assume that it wants us to pause
		 * execution until it has finished. */
		while(atomic_get(&cpu_pause_wait));
		return false;
	}

	_fatal(frame, "Received unexpected NMI");
}

/** Handler for page faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Whether to reschedule. */
static bool intr_handle_pagefault(unative_t num, intr_frame_t *frame) {
	int reason = (frame->err_code & (1<<0)) ? VM_FAULT_PROTECTION : VM_FAULT_NOTPRESENT;
	int access = (frame->err_code & (1<<1)) ? VM_FAULT_WRITE : VM_FAULT_READ;
	ptr_t addr = x86_read_cr2();

#if CONFIG_X86_NX
	/* Check if the fault was caused by instruction execution. */
	if(cpu_features.xd && frame->err_code & (1<<4)) {
		access = VM_FAULT_EXEC;
	}
#endif
	/* Check if a reserved bit fault. This is always fatal. */
	if(frame->err_code & (1<<3)) {
		fatal("Reserved bit pagefault exception (%p) (0x%x)", addr, frame->err_code);
	}

	/* Try the virtual memory manager if the fault occurred at a userspace
	 * address. */
	if(addr < (USER_MEMORY_BASE + USER_MEMORY_SIZE)) {
		if(vm_fault(addr, reason, access)) {
			return false;
		} else if(curr_thread->in_usermem) {
			kprintf(LOG_DEBUG, "arch: pagefault in usermem at %p (ip: %p)\n", addr, frame->ip);
			kdbg_enter(KDBG_ENTRY_USER, frame);
			context_restore_frame(&curr_thread->usermem_context, frame);
			return false;
		}
	}

	/* Nothing could handle this fault. If it happened in the kernel,
	 * die, otherwise just kill the process. */
	if(frame->err_code & (1<<2)) {
		kprintf(LOG_DEBUG, "arch: killing process %" PRId32 " due to unhandled pagefault (%p)\n",
		        curr_proc->id, addr);
		kprintf(LOG_DEBUG, "arch:  %s | %s%s%s\n",
		        (frame->err_code & (1<<0)) ? "protection" : "not-present",
		        (frame->err_code & (1<<1)) ? "write" : "read",
		        (frame->err_code & (1<<3)) ? " | reserved-bit" : "",
		        (frame->err_code & (1<<4)) ? " | execute" : "");
		kdbg_enter(KDBG_ENTRY_USER, frame);
		process_exit(255);
	} else {
		_fatal(frame, "Unhandled kernel-mode pagefault exception (%p)\n"
		              "%s | %s%s%s", addr,
		              (frame->err_code & (1<<0)) ? "Protection" : "Not-present",
		              (frame->err_code & (1<<1)) ? "Write" : "Read",
		              (frame->err_code & (1<<3)) ? " | Reserved-bit" : "",
		              (frame->err_code & (1<<4)) ? " | Execute" : "");
	}
}

/** Handler for device-not-available exceptions.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Whether to reschedule. */
static bool intr_handle_nm(unative_t num, intr_frame_t *frame) {
	if(frame->cs & 3) {
		fpu_request();
		return false;
	} else {
		_fatal(frame, "Unhandled kernel-mode exception %" PRIun " (%s)",
		       num, fault_names[num]);
	}
}

/** Handler for double faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Doesn't return. */
static bool intr_handle_doublefault(unative_t num, intr_frame_t *frame) {
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
	_fatal(frame, "Double Fault (%p)", frame->ip);
	cpu_halt();
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

/** Interrupt handler routine.
 * @param frame		Interrupt stack frame. */
void intr_handler(intr_frame_t *frame) {
	unative_t num = frame->int_no;
	bool schedule;

	/* Do entry stuff if coming from user mode. */
	if(frame->cs & 3) {
		thread_at_kernel_entry();
	}

	if(unlikely(atomic_get(&kdbg_running) == 2)) {
		kdbg_except_handler(num, (num < 32) ? fault_names[num] : "Unknown", frame);
		return;
	} else if(unlikely(!intr_handlers[num])) {
		if(num < 32) {
			/* Fatal if in kernel-mode, exit if in user-mode. */
			if(frame->cs & 3) {
				kprintf(LOG_DEBUG, "arch: killing process %" PRId32 " due to exception %" PRIun "\n",
				        curr_proc->id, num);
				kdbg_enter(KDBG_ENTRY_USER, frame);
				process_exit(255);
			} else {
				_fatal(frame, "Unhandled kernel-mode exception %" PRIun " (%s)",
				       num, fault_names[num]);
			}
		} else {
			_fatal(frame, "Recieved unknown interrupt %" PRIun, num);
		}
	}

	schedule = intr_handlers[num](num, frame);

	/* Do userspace return work if returning to userspace. This is done
	 * before rescheduling so that the thread does not stay around longer
	 * than necessary if it has been killed. */
	if(frame->cs & 3) {
		thread_at_kernel_exit();
	}

	/* Reschedule if asked to by the interrupt handler. */
	if(schedule) {
		sched_yield();
	}
}

/** Initialise the interrupt handler table. */
void intr_init(void) {
	int i;

	/* Set handlers for faults that require specific handling. */
	intr_register(FAULT_DEBUG, kdbg_int1_handler);
	intr_register(FAULT_NMI, intr_handle_nmi);
	intr_register(FAULT_DEVICE_NOT_AVAIL, intr_handle_nm);
	intr_register(FAULT_DOUBLE, intr_handle_doublefault);
	intr_register(FAULT_PAGE, intr_handle_pagefault);

	/* Entries 32-47 are IRQs, 48 onwards are unrecognised for now. */
	for(i = 32; i <= 47; i++) {
		intr_register(i, irq_handler);
	}

	irq_init();
}
