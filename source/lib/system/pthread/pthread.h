/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX threads private defintion.
 */

#pragma once

#include "libsystem.h"

#include <pthread.h>
#include <stdatomic.h>

struct __pthread {
    handle_t handle;            /**< Kernel thread handle. */

    /**
     * Reference count. The reason this structure exists rather than just making
     * pthread_t a handle_t directly is that the handle needs to exist past
     * pthread_detach() for pthread_self() to be able to return it if the thread
     * continues running, and the handle needs to continue existing after the
     * thread exits but before pthread_join() is called.
     *
     * This is handled by wrapping the handle in a reference counted structure.
     */
    atomic_int refcount;

    /** Entry/exit details. */
    void *(*start_routine)(void *);
    void *arg;
    void *exit_value;
};
