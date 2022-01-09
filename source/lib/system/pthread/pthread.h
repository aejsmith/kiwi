/*
 * Copyright (C) 2009-2022 Alex Smith
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
