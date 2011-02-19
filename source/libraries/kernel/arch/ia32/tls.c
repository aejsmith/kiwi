/*
 * Copyright (C) 2010 Alex Smith
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
