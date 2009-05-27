/* Kiwi x86 fault handling functions
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
 * @brief		x86 fault handling functions.
 *
 * The functions in this file are used to handle CPU exceptions. There is a
 * wrapper handler, fault_handler(), that gets called for all exceptions. If
 * a specific handler is specified in the table, then it will be called. It
 * should be noted that the return value for these functions are used to
 * specify whether the fault was handled successfully, NOT whether the current
 * process should be preempted.
 */

#include <arch/memmap.h>
#include <arch/page.h>
#include <arch/x86/fault.h>
#include <arch/x86/features.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <mm/aspace.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern atomic_t cpu_pause_wait;
extern atomic_t cpu_halting_all;

extern bool kdbg_int1_handler(unative_t num, intr_frame_t *frame);

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
static bool fault_handle_nmi(unative_t num, intr_frame_t *frame) {
	if(atomic_get(&cpu_halting_all)) {
		cpu_halt();
	} else if(atomic_get(&cpu_pause_wait)) {
		/* A CPU is in KDBG, assume that it wants us to pause
		 * execution until it has finished. */
		while(atomic_get(&cpu_pause_wait));
		return true;
	}

	return false;
}

/** Handler for double faults.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Doesn't return. */
static bool fault_handle_doublefault(unative_t num, intr_frame_t *frame) {
	/* Disable KDBG. */
	atomic_set(&kdbg_running, 3);

	_fatal(frame, "Double Fault (0x%p)", frame->ip);
	cpu_halt();
}

/** Handler for page faults.
 * @param num		CPU Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns true. */
static bool fault_handle_pagefault(unative_t num, intr_frame_t *frame) {
	int reason = (frame->err_code & (1<<0)) ? PF_REASON_PROT : PF_REASON_NPRES;
	int access = (frame->err_code & (1<<1)) ? PF_ACCESS_WRITE : PF_ACCESS_READ;
	ptr_t addr = sysreg_cr2_read();

#if CONFIG_X86_NX
	/* Check if the fault was caused by instruction execution. */
	if(CPU_HAS_XD(curr_cpu) && frame->err_code & (1<<4)) {
		reason = PF_ACCESS_EXEC;
	}
#endif

	/* Handle exceptions during KDBG execution. We should not call into
	 * the address space manager if we are in KDBG. */
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, "Page Fault", frame);
		return true;
	}

	/* Try the address space manager if the fault occurred at a userspace
	 * address. */
	if(addr < (ASPACE_BASE + ASPACE_SIZE)) {
		if(aspace_pagefault(addr, reason, access) == PF_STATUS_OK) {
			return true;
		}
	}

	/* Nothing could handle this fault, drop dead. */
	_fatal(frame, "Unhandled %s-mode pagefault exception (0x%p)\n"
	              "%s | %s %s %s",
	              (frame->err_code & (1<<2)) ? "user" : "kernel", addr,
	              (frame->err_code & (1<<0)) ? "Protection" : "Not-present",
	              (frame->err_code & (1<<1)) ? "Write" : "Read",
	              (frame->err_code & (1<<3)) ? " | Reserved-bit" : "",
	              (frame->err_code & (1<<4)) ? " | Execute" : "");
}

/** Table of special fault handlers. */
static intr_handler_t fault_handler_table[] = {
	[FAULT_DEBUG] = kdbg_int1_handler,
	[FAULT_NMI] = fault_handle_nmi,
	[FAULT_DOUBLE] = fault_handle_doublefault,
	[FAULT_PAGE] = fault_handle_pagefault,
};

/** Handle a CPU exception.
 *
 * Handler for all CPU exceptions. If there is a specific handler for the
 * exception, it is called, else the standard action is performed.
 *
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 *
 * @return		Whether the current process should be preeempted.
 */
bool fault_handler(unative_t num, intr_frame_t *frame) {
	/* KDBG is fully running on this CPU (or at least we hope so...).
	 * Have it handle the fault itself. */
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, fault_names[num], frame);
		return false;
	}

	/* If there is a special handler for this fault run it. */
	if(fault_handler_table[num]) {
		if(fault_handler_table[num](num, frame)) {
			return false;
		}
	}

	/* No specific handler or the handler did not handle the fault. */
	_fatal(frame, "Unhandled kernel-mode exception %" PRIun " (%s)", num, fault_names[num]);
}
