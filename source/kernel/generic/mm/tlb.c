/* Kiwi TLB shootdown functions
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
 * @brief		TLB shootdown functions.
 *
 * Reference:
 * - Translation Lookaside Buffer Consistency: A Software Approach
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.92.1801
 */

#include <cpu/intr.h>

#include <mm/aspace.h>
#include <mm/tlb.h>

#include <assert.h>
#include <errors.h>

/** Begin TLB shootdown.
 *
 * Starts the TLB shootdown process. First invalidates the TLB on the current
 * CPU if required, and then interrupts all other CPUs using the address space
 * to cause them to enter tlb_shootdown_responder(). If the address space is
 * specified as NULL, then the TLB will be invalidated on every CPU (i.e.
 * when operating on the kernel page map). This function should be called
 * before making any changes to a page map on SMP systems.
 *
 * @param msg		Message structure to use, should later be passed to
 *			tlb_shootdown_finalise().
 * @param as		Address space to invalidate.
 * @param start		Start of address range to invalidate.
 * @param end		End of address range to invalidate.
 */
void tlb_shootdown_initiator(tlb_shootdown_t *msg, aspace_t *as, ptr_t start, ptr_t end) {
	cpu_t *cpu;

	/* Invalidate on the calling CPU if required. */
	if(as == NULL || as == curr_aspace) {
		tlb_invalidate(start, end);
	}

	/* Quick check to see if we need to do anything, without going through
	 * the loop. */
	if(as != NULL) {
		if(refcount_get(&as->count) == ((as == curr_aspace) ? 1 : 0)) {
			return;
		}
	}

	msg->as = as;
	msg->start = start;
	msg->end = end;

	/* There are other users of the address space. Need to deliver a TLB
	 * shootdown request to them. */
	LIST_FOREACH(&cpus_running, iter) {
		cpu = list_entry(iter, cpu_t, header);

		if(as != NULL && cpu->aspace != as) {
			continue;
		}

		/* CPU is using this address space. */
	}
}

void tlb_shootdown_finalize(tlb_shootdown_t *msg) {

}

bool tlb_shootdown_responder(unative_t num, intr_frame_t *frame) {
	return false;
}
