/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Inter-Processor Interrupt functions.
 */

#ifndef __CPI_IPI_H
#define __CPI_IPI_H

#if CONFIG_SMP

#include <cpu/cpu.h>

#include <types.h>

/** Flags to modify IPI sending behaviour. */
#define IPI_SEND_SYNC		(1<<0)	/**< Send message synchronously rather than asynchronously. */

/** Type of a function to handle an IPI. */
typedef status_t (*ipi_handler_t)(void *, unative_t, unative_t, unative_t, unative_t);

extern void ipi_arch_interrupt(cpu_id_t dest);

extern status_t ipi_send(cpu_id_t dest, ipi_handler_t handler, unative_t data1, unative_t data2,
                         unative_t data3, unative_t data4, int flags);
extern void ipi_broadcast(ipi_handler_t handler, unative_t data1, unative_t data2,
                          unative_t data3, unative_t data4, int flags);
extern void ipi_acknowledge(void *message, status_t status);

extern void ipi_init(void);

#endif /* CONFIG_SMP */
#endif /* __CPI_IPI_H */
