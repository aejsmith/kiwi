/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		IA32 thread-local storage support functions.
 */

#include "../../libkernel.h"

/** Argument passed to ___tls_get_addr(). */
typedef struct tls_index {
	unsigned long int ti_module;
	unsigned long int ti_offset;
} tls_index_t;

extern void *___tls_get_addr(tls_index_t *index) __attribute__((__regparm__(1))) __export;

/** IA32-specific TLS address lookup function.
 * @param index		Pointer to argument structure. */
__attribute__((__regparm__(1))) __export void *___tls_get_addr(tls_index_t *index) {
	return tls_get_addr(index->ti_module, index->ti_offset);
}

/** Initialise architecture-specific data in the TCB.
 * @param tcb		Thread control block. */
void tls_tcb_init(tls_tcb_t *tcb) {
	/* The base of the GS segment is set to point to the start of the TCB.
	 * The first 4 bytes in the TCB must contain the linear address of the
	 * TCB, so that it can be obtained at %gs:0. */
	tcb->tpt = tcb;
}
