/* Kiwi x86 pagefault handler
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
 * @brief		x86 paging functions.
 */

#include <arch/features.h>
#include <arch/mem.h>
#include <arch/page.h>

#include <console/kprintf.h>

#include <mm/aspace.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern bool pagefault_handler(unative_t num, intr_frame_t *regs);

/** Get string representation of a fault reason.
 * @param reason	Fault reason.
 * @return		String representation of reason. */
static inline const char *pagefault_reason(int reason) {
	switch(reason) {
	case PF_REASON_NPRES:	return "Not-present";
	case PF_REASON_PROT:	return "Protection";
	}

	return "Unknown";
}

/** Get string representation of a fault access.
 * @param access	Fault access.
 * @return		String representation of access. */
static inline const char *pagefault_access(int access) {
	switch(access) {
	case PF_ACCESS_READ:	return "Read";
	case PF_ACCESS_WRITE:	return "Write";
	case PF_ACCESS_EXEC:	return "Execute";
	}

	return "Unknown";
}

/** Handler for a page fault.
 *
 * Handler for a page fault - decodes the exception error code and asks the
 * address space manager to handle the fault.
 *
 * @param num		Interrupt number.
 * @param regs		Pointer to interrupt frame.
 *
 * @return		Whether the current thread should be preempted.
 */
bool pagefault_handler(unative_t num, intr_frame_t *regs) {
	int reason = (regs->err_code & (1<<0)) ? PF_REASON_PROT : PF_REASON_NPRES;
	int access = (regs->err_code & (1<<1)) ? PF_ACCESS_WRITE : PF_ACCESS_READ;
	ptr_t addr = read_cr2();

#if CONFIG_X86_NX
	/* Check if the fault was caused by instruction execution. */
	if(CPU_HAS_XD(curr_cpu) && regs->err_code & (1<<4)) {
		reason = PF_ACCESS_EXEC;
	}
#endif

	/* Handle exceptions during KDBG execution. We should not call into
	 * the address space manager if we are in KDBG. */
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, "Page Fault", regs);
		return false;
	}

	/* Try the address space manager if the fault occurred at a userspace
	 * address. */
	if(addr < (USPACE_BASE + USPACE_SIZE)) {
		if(aspace_pagefault(addr, reason, access) == PF_STATUS_OK) {
			return false;
		}
	}

	/* Nothing could handle this fault, drop dead. */
	_fatal(regs, "Unhandled %s-mode pagefault exception (0x%p)\n"
	             "%s | %s %s %s",
	             (regs->err_code & (1<<2)) ? "user" : "kernel", addr,
	             (regs->err_code & (1<<0)) ? "Protection" : "Not-present",
	             (regs->err_code & (1<<1)) ? "Write" : "Read",
	             (regs->err_code & (1<<3)) ? " | Reserved-bit" : "",
	             (regs->err_code & (1<<4)) ? " | Execute" : "");
}
