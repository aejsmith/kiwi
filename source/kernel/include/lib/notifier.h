/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
