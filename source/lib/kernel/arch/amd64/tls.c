/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
