/* Kiwi x86 IPI functions
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
 * @brief		x86 IPI functions.
 */

#ifndef __ARCH_X86_IPI_H
#define __ARCH_X86_IPI_H

#include <arch/x86/lapic.h>
#include <arch/x86/cpu.h>

/** Send an IPI interrupt to a single CPU.
 * @param dest		Destination CPU ID. */
static inline void ipi_send_interrupt(cpu_id_t dest) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)dest, LAPIC_IPI_FIXED, LAPIC_VECT_IPI);
}

#endif /* __ARCH_X86_IPI_H */
