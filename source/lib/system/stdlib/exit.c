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
 * @brief               Exit functions.
 */

#include <core/list.h>
#include <core/mutex.h>

#include <kernel/process.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "libsystem.h"

extern void *__dso_handle __attribute__((__weak__));

extern int __cxa_atexit(void (*function)(void *), void *arg, void *dso);
extern void __cxa_finalize(void *d);

/** Structure defining an at-exit function. */
typedef struct atexit_func {
    core_list_t header;             /**< List header. */

    void (*func)(void *);           /**< Function pointer. */
    void *arg;                      /**< Function argument. */
    void *dso;                      /**< DSO handle. */
} atexit_func_t;

/** Statically allocated structures. */
static atexit_func_t atexit_array[ATEXIT_MAX];
static bool atexit_inited;

/** List of free at-exit functions. */
static CORE_LIST_DEFINE(atexit_free_funcs);
static size_t atexit_free_count;

/** List of registered at-exit functions. */
static CORE_LIST_DEFINE(atexit_funcs);
static size_t atexit_count;

/** Locking to protect at-exit lists. */
static CORE_MUTEX_DEFINE(atexit_lock);

/** Allocate an at-exit function structure.
 * @return              Function structure pointer, or NULL if none free. */
static atexit_func_t *atexit_alloc(void) {
    atexit_func_t *func = NULL;
    size_t i;

    if (!atexit_inited) {
        for (i = 0; i < ATEXIT_MAX; i++) {
            core_list_init(&atexit_array[i].header);
            core_list_append(&atexit_free_funcs, &atexit_array[i].header);
            atexit_free_count++;
        }

        atexit_inited = true;
    }

    if (atexit_free_count) {
        if (core_list_empty(&atexit_free_funcs))
            libsystem_fatal("atexit data is corrupted");

        func = core_list_entry(atexit_free_funcs.next, atexit_func_t, header);
        core_list_remove(&func->header);
    }

    return func;
}

/** Free an at-exit function structure.
 * @param func          Function structure. */
static void atexit_free(atexit_func_t *func) {
    core_list_append(&atexit_free_funcs, &func->header);
    atexit_free_count++;
}

/** Register a C++ cleanup function.
 * @param function      Function to call.
 * @param arg           Argument.
 * @param dso           DSO handle.
 * @return              0 on success, -1 on failure. */
int __cxa_atexit(void (*function)(void *), void *arg, void *dso) {
    atexit_func_t *func;

    core_mutex_lock(&atexit_lock, -1);

    if (!(func = atexit_alloc())) {
        core_mutex_unlock(&atexit_lock);
        return -1;
    }

    func->func = (void (*)(void *))function;
    func->arg = arg;
    func->dso = dso;

    core_list_prepend(&atexit_funcs, &func->header);
    atexit_count++;

    core_mutex_unlock(&atexit_lock);
    return 0;
}

/** Run C++ cleanup functions.
 * @param d             DSO handle, if NULL will call all handlers. */
void __cxa_finalize(void *d) {
    atexit_func_t *func;
    size_t count;

restart:
    core_list_foreach_safe(&atexit_funcs, iter) {
        func = core_list_entry(iter, atexit_func_t, header);
        count = atexit_count;

        if (!d || d == func->dso) {
            func->func(func->arg);

            if (atexit_count != count) {
                atexit_free(func);
                goto restart;
            } else {
                atexit_free(func);
            }
        }
    }
}

/**
 * Define a function to run at process exit.
 *
 * Defines a function to be run at normal (i.e. invocation of exit())
 * process termination. Use of _exit() or _Exit(), or involuntary process
 * termination, will not result in functions registered with this function
 * being called.
 *
 * @param function      Function to define.
 *
 * @return              0 on success, -1 on failure.
 */
int atexit(void (*function)(void)) {
    return __cxa_atexit((void (*)(void *))function, NULL, (&__dso_handle) ? __dso_handle : NULL);
}

/**
 * Call at-exit functions and terminate execution.
 *
 * Calls all functions previously defined with atexit() and terminates
 * the process.
 *
 * @param status        Exit status.
 *
 * @return              Does not return.
 */
void exit(int status) {
    __cxa_finalize(NULL);
    kern_process_exit(status);
}

/** Terminate execution without calling at-exit functions.
 * @param status        Exit status.
 * @return              Does not return. */
void _exit(int status) {
    kern_process_exit(status);
}

/** Terminate execution without calling at-exit functions.
 * @param status        Exit status.
 * @return              Does not return. */
void _Exit(int status) {
    kern_process_exit(status);
}
