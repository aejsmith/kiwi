/* Kiwi IPI functions
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
 * @brief		Inter-Processor Interrupt functions.
 */

#ifndef __CPI_IPI_H
#define __CPI_IPI_H

#include <types.h>

/** Flags to modify IPI sending behaviour. */
#define IPI_SEND_SYNC		(1<<0)	/**< Send message synchronously rather than asynchronously. */

/** Type of a function to handle an IPI. */
typedef int (*ipi_handler_t)(void *, unative_t, unative_t, unative_t, unative_t);

extern int ipi_send(cpu_id_t dest, ipi_handler_t handler, unative_t data1, unative_t data2,
                    unative_t data3, unative_t data4, int flags);
extern void ipi_broadcast(ipi_handler_t handler, unative_t data1, unative_t data2,
                          unative_t data3, unative_t data4, int flags);
extern void ipi_acknowledge(void *message, int status);

extern void ipi_init(void);

#endif /* __CPI_IPI_H */
