/*
 * Copyright (C) 2010-2014 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               AMD64 thread-local storage support functions.
 */

#include "libkernel.h"

/** Argument passed to __tls_get_addr(). */
typedef struct tls_index {
    unsigned long ti_module;
    unsigned long ti_offset;
} tls_index_t;

extern void *__tls_get_addr(tls_index_t *index) __sys_export;

/** AMD64-specific TLS address lookup function.
 * @param index         Pointer to argument structure. */
void *__tls_get_addr(tls_index_t *index) {
    return tls_get_addr(index->ti_module, index->ti_offset);
}
