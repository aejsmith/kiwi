/* Kiwi x86 interrupt functions
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
 * @brief		x86 interrupt functions.
 */

#include <arch/memmap.h>
#include <arch/page.h>
#include <arch/x86/features.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <lib/utility.h>

#include <mm/vm.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern atomic_t cpu_pause_wait;
extern atomic_t cpu_halting_all;

extern intr_result_t kdbg_int1_handler(unative_t num, intr_frame_t *frame);
extern void intr_handler(unative_t num, intr_frame_t *frame);

/** Array of interrupt handling routines. */
static intr_handler_t intr_handlers[INTR_COUNT];

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
 * @return		True if handled, false if not. */
static intr_result_t intr_handle_nmi(unative_t num, intr_frame_t *frame) {
	if(atomic_get(&cpu_halting_all)) {
		cpu_halt();
	} else if(atomic_get(&cpu_pause_wait)) {
		/* A CPU is in KDBG, assume that it wants us to pause
		 * execution until it has finished. */
		while(atomic_get(&cpu_pause_wait));
		return INTR_HANDLED;
	}

	_fatal(frame, "Received unexpected NMI");
}

/** Handler for page faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Interrupt status code. */
static intr_result_t intr_handle_pagefault(unative_t num, intr_frame_t *frame) {
	int reason = (frame->err_code & (1<<0)) ? VM_FAULT_PROTECTION : VM_FAULT_NOTPRESENT;
	int access = (frame->err_code & (1<<1)) ? VM_FAULT_WRITE : VM_FAULT_READ;
	ptr_t addr = sysreg_cr2_read();

#if CONFIG_X86_NX
	/* Check if the fault was caused by instruction execution. */
	if(CPU_HAS_XD(curr_cpu) && frame->err_code & (1<<4)) {
		access = VM_FAULT_EXEC;
	}
#endif

	/* Try the virtual memory manager if the fault occurred at a userspace
	 * address. */
	if(addr < (ASPACE_BASE + ASPACE_SIZE)) {
		if(vm_fault(addr, reason, access) == VM_FAULT_HANDLED) {
			return true;
		} else if(atomic_get(&curr_thread->in_usermem)) {
			kprintf(LOG_DEBUG, "arch: pagefault in usermem at %p (ip: %p)\n", addr, frame->ip);
			context_restore_frame(&curr_thread->usermem_context, frame);
			return true;
		}
	}

	/* Nothing could handle this fault, drop dead. */
	_fatal(frame, "Unhandled %s-mode pagefault exception (%p)\n"
	              "%s | %s%s%s",
	              (frame->err_code & (1<<2)) ? "user" : "kernel", addr,
	              (frame->err_code & (1<<0)) ? "Protection" : "Not-present",
	              (frame->err_code & (1<<1)) ? "Write" : "Read",
	              (frame->err_code & (1<<3)) ? " | Reserved-bit" : "",
	              (frame->err_code & (1<<4)) ? " | Execute" : "");
}

/** Handler for double faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Doesn't return. */
static intr_result_t intr_handle_doublefault(unative_t num, intr_frame_t *frame) {
#if !CONFIG_ARCH_AMD64
	/* Disable KDBG on IA32. */
	atomic_set(&kdbg_running, 3);
#endif

	/* Crappy workaround, using MMX memcpy() from the console code seems
	 * to cause nasty problems. */
	curr_cpu->arch.features.feat_edx &= ~(1<<23);
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
	assert(num < INTR_COUNT);
	intr_handlers[num] = handler;
}

/** Remove an interrupt handler.
 *
 * Unregisters an interrupt handler.
 *
 * @param num		Interrupt number.
 */
void intr_remove(unative_t num) {
	assert(num < INTR_COUNT);
	intr_handlers[num] = NULL;
}

/** Interrupt handler routine.
 *
 * Handles a CPU interrupt by looking up the handler routine in the handler
 * table and calling it.
 *
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 */
void intr_handler(unative_t num, intr_frame_t *frame) {
	intr_handler_t handler = intr_handlers[num];

	if(num < 32 && unlikely(atomic_get(&kdbg_running) == 2)) {
		kdbg_except_handler(num, fault_names[num], frame);
		return;
	} else if(unlikely(!handler)) {
		if(num < 32) {
			_fatal(frame, "Unhandled %s-mode exception %" PRIun " (%s)",
			       (frame->cs & 3) ? "user" : "kernel", num,
			       fault_names[num]);
		} else {
			_fatal(frame, "Recieved unknown interrupt %" PRIun, num);
		}
	}

	if(handler(num, frame) == INTR_RESCHEDULE) {
		sched_yield();
	}
}

/** Initialize the interrupt handling code. */
void intr_init(void) {
	int i;

	/* Set handlers for faults that require specific handling. */
	intr_register(FAULT_DEBUG, kdbg_int1_handler);
	intr_register(FAULT_NMI, intr_handle_nmi);
	intr_register(FAULT_DOUBLE, intr_handle_doublefault);
	intr_register(FAULT_PAGE, intr_handle_pagefault);

	/* Entries 32-47 are IRQs, 48 onwards are unrecognised for now. */
	for(i = 32; i <= 47; i++) {
		intr_register(i, irq_handler);
	}

	irq_init();
}
