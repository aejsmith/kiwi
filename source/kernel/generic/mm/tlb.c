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

#include <arch/spinlock.h>

#include <cpu/ipi.h>

#include <mm/aspace.h>
#include <mm/page.h>
#include <mm/tlb.h>

#include <assert.h>
#include <fatal.h>

/** TLB shootdown responder.
 * @param msg		IPI message structure.
 * @param data1		Address of address space.
 * @param data2		Start of range to invalidate.
 * @param data3		End of range to invalidate.
 * @param data4		Unused.
 * @return		Always returns 0. */
static int tlb_shootdown_responder(void *msg, unative_t data1, unative_t data2,
                                   unative_t data3, unative_t data4) {
	aspace_t *as = (aspace_t *)data1;
	ptr_t start = (ptr_t)data2;
	ptr_t end = (ptr_t)data3;
	page_map_t *pmap;

	/* Acknowledge receipt of the message. */
	ipi_acknowledge(msg, 0);

	/* We may have changed address space between the IPI being sent and
	 * receiving it, check if we need to do anything. */
	if(as != NULL && as != curr_aspace) {
		return 0;
	}

	/* Wait for the page map to become unlocked. */
	pmap = (as != NULL) ? &as->pmap : &kernel_page_map;
	while(page_map_locked(pmap)) {
		spinlock_loop_hint();
	}

	/* Perform the required action. */
	tlb_invalidate(start, end);
	return 0;
}

/** Begin TLB shootdown.
 *
 * Starts the TLB shootdown process. First invalidates the TLB on the current
 * CPU if required, and then sends an IPI to all other CPUs using the address
 * space to cause them to enter tlb_shootdown_responder(). If the address space
 * is specified as NULL, then the TLB will be invalidated on every CPU (i.e.
 * when operating on the kernel page map).
 *
 * The CPUs, after acknowledging receipt of the IPI, will spin waiting for
 * the page map lock to be released before return
 *
 * @todo		Would it be better to do like ipi_broadcast() but to
 *			only the CPUs we want?
 *
 * @param msg		Message structure to use, should later be passed to
 *			tlb_shootdown_finalise().
 * @param as		Address space to invalidate.
 * @param start		Start of address range to invalidate.
 * @param end		End of address range to invalidate.
 */
void tlb_shootdown(aspace_t *as, ptr_t start, ptr_t end) {
	cpu_t *cpu;

	/* Invalidate on the calling CPU if required. */
	if(as == NULL || as == curr_aspace) {
		tlb_invalidate(start, end);
	}

	/* Quick check to see if we need to do anything, without going through
	 * the loop. */
	if(cpu_count < 2 || (as != NULL && refcount_get(&as->count) == ((as == curr_aspace) ? 1 : 0))) {
		return;
	}

	/* There are other users of the address space. Need to deliver a TLB
	 * shootdown request to them. */
	LIST_FOREACH(&cpus_running, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu == curr_cpu || (as != NULL && cpu->aspace != as)) {
			continue;
		}

		/* CPU is using this address space. */
		if(ipi_send(cpu->id, tlb_shootdown_responder, (unative_t)as,
		            (unative_t)start, (unative_t)end, 0,
		            IPI_SEND_SYNC) != 0) {
			fatal("Could not send TLB shootdown IPI");
		}
	}
}
