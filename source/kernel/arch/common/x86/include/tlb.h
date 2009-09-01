/* Kiwi x86 TLB invalidation functions
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
 * @brief		x86 TLB invalidation functions.
 */

#ifndef __ARCH_X86_TLB_H
#define __ARCH_X86_TLB_H

#include <arch/page.h>
#include <arch/x86/sysreg.h>

/** Invalidate TLB entries for an address range.
 * @param start		Start of range to invalidate.
 * @param end		End of range to invalidate. */
static inline void tlb_arch_invalidate(ptr_t start, ptr_t end) {
	for(; start < end; start += PAGE_SIZE) {
		__asm__ volatile("invlpg (%0)" :: "r"(start));
	}
}

/** Invalidate the entire TLB. */
static inline void tlb_arch_invalidate_all(void) {
	sysreg_cr3_write(sysreg_cr3_read());
}

#endif /* __ARCH_X86_TLB_H */
