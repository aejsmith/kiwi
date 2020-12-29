/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Semaphore implementation.
 */

#pragma once

#include <lib/list.h>

#include <sync/spinlock.h>

/** Structure containing a semaphore. */
typedef struct semaphore {
    size_t count;                   /**< Count of the semaphore. */
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t threads;                 /**< List of waiting threads. */
    const char *name;               /**< Name of the semaphore. */
} semaphore_t;

/** Initializes a statically defined semaphore. */
#define SEMAPHORE_INITIALIZER(_var, _name, _initial) \
    { \
        .count = _initial, \
        .lock = SPINLOCK_INITIALIZER("semaphore_lock"), \
        .threads = LIST_INITIALIZER(_var.threads), \
        .name = _name, \
    }

/** Statically define a new semaphore. */
#define SEMAPHORE_DEFINE(_var, _initial) \
    semaphore_t _var = SEMAPHORE_INITIALIZER(_var, #_var, _initial)

extern status_t semaphore_down_etc(semaphore_t *sem, nstime_t timeout, unsigned flags);
extern void semaphore_down(semaphore_t *sem);
extern void semaphore_up(semaphore_t *sem, size_t count);
extern void semaphore_init(semaphore_t *sem, const char *name, size_t initial);
