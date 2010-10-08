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
} libc_mutex_t;

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
