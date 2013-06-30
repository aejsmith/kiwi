/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		C library mutex implementation.
 */

#ifndef __UTIL_MUTEX_H
#define __UTIL_MUTEX_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure containing a mutex. */
typedef struct libc_mutex {
	volatile int32_t futex;		/**< Futex value. */
} __attribute__((aligned(4))) libc_mutex_t;

/** Initialises a statically declared mutex. */
#define LIBC_MUTEX_INITIALISER		{ 0 }

/** Statically declares a new mutex. */
#define LIBC_MUTEX_DECLARE(_var)	\
	libc_mutex_t _var = LIBC_MUTEX_INITIALISER

extern bool libc_mutex_held(libc_mutex_t *lock);
extern status_t libc_mutex_lock(libc_mutex_t *lock, useconds_t timeout);
extern void libc_mutex_unlock(libc_mutex_t *lock);
extern void libc_mutex_init(libc_mutex_t *lock);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_MUTEX_H */
