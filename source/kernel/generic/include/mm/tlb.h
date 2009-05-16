/* Kiwi TLB invalidation/shootdown functions
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
 * @brief		TLB invalidation/shootdown functions.
 */

#ifndef __MM_TLB_H
#define __MM_TLB_H

#include <arch/tlb.h>

#if CONFIG_SMP

#include <cpu/cpu.h>

#include <lib/refcount.h>

#include <mm/aspace.h>

#include <types.h>

/** TLB shootdown message structure. */
typedef struct tlb_shootdown {
	aspace_t *as;			/**< Address space (NULL implies kernel address space). */
	ptr_t start;			/**< Start of address range. */
	ptr_t end;			/**< End of address range. */

	refcount_t count;		/**< Reference count to track CPUs waiting on this message. */
} tlb_shootdown_t;

extern void tlb_shootdown_initiator(tlb_shootdown_t *msg, aspace_t *as, ptr_t start, ptr_t end);
extern void tlb_shootdown_finalize(tlb_shootdown_t *msg);
extern bool tlb_shootdown_responder(unative_t num, intr_frame_t *frame);

#endif /* CONFIG_SMP */

#endif /* __MM_TLB_H */
