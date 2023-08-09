/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Event notification system.
 */

#pragma once

#include <sync/mutex.h>

/** Notifier structure. */
typedef struct notifier {
    mutex_t lock;               /**< Lock to protect list. */
    list_t functions;           /**< Functions to call when the event occurs. */
    void *data;                 /**< Data to pass to functions. */
} notifier_t;

/** Initializes a statically declared notifier. */
#define NOTIFIER_INITIALIZER(_var, _data) \
    { \
        .lock = MUTEX_INITIALIZER(_var.lock, "notifier_lock", MUTEX_RECURSIVE), \
        .functions = LIST_INITIALIZER(_var.functions), \
        .data = _data, \
    }

/** Statically defines a new notifier. */
#define NOTIFIER_DEFINE(_var, _data) \
    notifier_t _var = NOTIFIER_INITIALIZER(_var, _data)

/** Check if a notifier's function list is empty.
 * @param notifier      Notifier to check.
 * @return              Whether the function list is empty. */
static inline bool notifier_empty(notifier_t *notifier) {
    return list_empty(&notifier->functions);
}

/** Notifier function type.
 * @param arg1          Data argument associated with the notifier.
 * @param arg2          Data argument registered with the function.
 * @param arg3          Data argument passed to notifier_run(). */
typedef void (*notifier_func_t)(void *arg1, void *arg2, void *arg3);

extern void notifier_init(notifier_t *notifier, void *data);
extern void notifier_clear(notifier_t *notifier);
extern bool notifier_run_unsafe(notifier_t *notifier, void *data, bool destroy);
extern bool notifier_run(notifier_t *notifier, void *data, bool destroy);
extern void notifier_register(notifier_t *notifier, notifier_func_t func, void *data);
extern void notifier_unregister(notifier_t *notifier, notifier_func_t func, void *data);
