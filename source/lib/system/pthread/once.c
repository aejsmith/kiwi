/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Single execution function.
 */

#include <pthread.h>

/**
 * Execute a function only once in any thread.
 *
 * The first thread to call this function on a given control variable will
 * execute the specified function. Any subsequent calls by this thread or any
 * other thread on the same control variable will do nothing.
 *
 * If func is a cancellation point and it is cancelled, the effect on the
 * control variable will be as if pthread_once() was never called.
 *
 * @param once          Pointer to control variable.
 * @param func          Function to call.
 *
 * @return              0 on success, error number on failure.
 */
int pthread_once(pthread_once_t *once, void (*func)(void)) {
    /* FIXME: Implement cancellation semantics - use cleanup functions. */
    if (__sync_bool_compare_and_swap((volatile int32_t *)once, 0, 1))
        func();

    return 0;
}
