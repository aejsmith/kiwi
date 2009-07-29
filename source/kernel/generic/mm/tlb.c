/* Kiwi TLB invalidation functions
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
 * @brief		TLB invalidation functions.
 */

#include <cpu/ipi.h>

#include <mm/aspace.h>
#include <mm/page.h>
#include <mm/tlb.h>

#include <assert.h>
#include <fatal.h>

/** TLB invalidation IPI handler.
 * @param msg		IPI message structure.
 * @param data1		Address of address space.
 * @param data2		Start of range to invalidate.
 * @param data3		End of range to invalidate.
 * @param data4		Unused.
 * @return		Always returns 0. */
static int tlb_invalidate_handler(void *msg, unative_t data1, unative_t data2,
                                  unative_t data3, unative_t data4) {
	aspace_t *as = (aspace_t *)data1;
	ptr_t start = (ptr_t)data2;
	ptr_t end = (ptr_t)data3;

	/* We may have changed address space between the IPI being sent and
	 * receiving it, check if we need to do anything. */
	if(as != NULL && as != curr_aspace) {
		return 0;
	}

	/* Perform the required action. */
	tlb_arch_invalidate(start, end);
	return 0;
}

/** Invalidate TLB entries.
 *
 * Invalidates the given address range in the TLB of all CPUs using an
 * address space.
 *
 * @todo		Implement ipi_multicast()
 *
 * @param as		Address space to invalidate in (if NULL, will
 *			invalidate in all CPUs, i.e. operating on kernel page
 *			map).
 * @param start		Start of address range to invalidate.
 * @param end		End of address range to invalidate.
 */
void tlb_invalidate(aspace_t *as, ptr_t start, ptr_t end) {
	cpu_t *cpu;

	/* Invalidate on the calling CPU if required. */
	if(as == NULL || as == curr_aspace) {
		tlb_arch_invalidate(start, end);
	}

	/* Don't need to do anything with 1 CPU. */
	if(cpu_count < 2) {
		return;
	}

	if(as != NULL) {
		if(refcount_get(&as->count) == ((as == curr_aspace) ? 1 : 0)) {
			return;
		}

		/* There are other users of the address space. Need to deliver a TLB
		 * invalidation request to them. */
		LIST_FOREACH(&cpus_running, iter) {
			cpu = list_entry(iter, cpu_t, header);
			if(cpu == curr_cpu || (as != NULL && cpu->aspace != as)) {
				continue;
			}

			/* CPU is using this address space. */
			if(ipi_send(cpu->id, tlb_invalidate_handler, (unative_t)as,
			            (unative_t)start, (unative_t)end, 0,
			            IPI_SEND_SYNC) != 0) {
				fatal("Could not send TLB invalidation IPI");
			}
		}
	} else {
		ipi_broadcast(tlb_invalidate_handler, 0, (unative_t)start, (unative_t)end, 0, IPI_SEND_SYNC);
	}
}
