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
 * @brief               Main POSIX thread functions.
 *
 * See TODOs scattered around for things to do when we actually have threads
 * implemented.
 */

#include "pthread/pthread.h"

#include "libsystem.h"

#include <kernel/object.h>
#include <kernel/private/thread.h>
#include <kernel/status.h>

#include <errno.h>
#include <stdlib.h>

/**
 * Pointer to self. If the thread is created by pthread_create(), we'll set this
 * on thread entry to the thread object created there. To support pthread_self()
 * on non-pthread threads, we'll create a wrapper on-demand if necessary.
 */
static __thread pthread_t pthread_self_pointer = NULL;

static void pthread_release(pthread_t thread) {
    if (atomic_fetch_sub_explicit(&thread->refcount, 1, memory_order_acq_rel) == 1) {
        kern_handle_close(thread->handle);
        free(thread);
    }
}

static void release_pthread_self(void) {
    if (pthread_self_pointer) {
        pthread_release(pthread_self_pointer);
        pthread_self_pointer = NULL;
    }
}

static __sys_init_prio(LIBSYSTEM_INIT_PRIO_PTHREAD) void pthread_init(void) {
    status_t ret = kern_thread_add_dtor(release_pthread_self);
    libsystem_assert(ret == STATUS_SUCCESS);
}

static int pthread_entry(void *arg) {
    pthread_self_pointer = arg;

    pthread_self_pointer->exit_value =
        pthread_self_pointer->start_routine(pthread_self_pointer->arg);

    /* For a pthread we expect that the return value pointer will be picked up
     * via pthread APIs so don't attempt to mash the pointer into the integer
     * kernel exit status. */
    return 0;
}

/**
 * Create a new thread in the calling process. The thread will begin execution
 * at the specified function.
 *
 * @param _thread       Where to store ID of created thread.
 * @param attr          Thread creation attributes (see pthread_attr_*). If
 *                      NULL, default attributes will be used.
 * @param start_routine Thread entry point.
 * @param arg           Argument to pass to start_routine.
 *
 * @return              0 on success.
 *                      EAGAIN if the resources necessary to create a thread
 *                      are unavailable.
 */
int pthread_create(
    pthread_t *__restrict _thread, const pthread_attr_t *__restrict attr,
    void *(*start_routine)(void *), void *__restrict arg)
{
    /* TODO. */
    libsystem_assert(!attr);

    pthread_t thread = malloc(sizeof(*thread));
    if (!thread)
        return EAGAIN;

    /* 2 references for what we return and the thread's pthread_self pointer. */
    atomic_store_explicit(&thread->refcount, 2, memory_order_relaxed);

    thread->start_routine = start_routine;
    thread->arg           = arg;
    thread->exit_value    = NULL;

    status_t ret = kern_thread_create("pthread", pthread_entry, thread, NULL, 0, &thread->handle);
    if (ret != STATUS_SUCCESS) {
        free(thread);

        /* POSIX only specifies EAGAIN as an error for any kind of lack of
         * resources which covers most of the reasons this can fail. TODO: Might
         * have more failure reasons with attributes implemented */
        return EAGAIN;
    }

    *_thread = thread;
    return 0;
}

/**
 * Indicate that the resources for a thread can be released as soon as the
 * thread finishes execution. Once the thread exits, the thread ID immediately
 * becomes invalid and any subsequent use of it is undefined behaviour.
 *
 * @param thread        Thread to detach.
 */
int pthread_detach(pthread_t thread) {
    pthread_release(thread);
    return 0;
}

/** Determine whether 2 POSIX thread IDs are equal.
 * @param p1            First thread ID.
 * @param p2            Second thread ID.
 * @return              Non-zero if thread IDs are equal, zero otherwise. */
int pthread_equal(pthread_t p1, pthread_t p2) {
    return p1 == p2;
}

/**
 * Exit the current thread. This can be used from threads created by native
 * kernel thread APIs, however in that case the exit value will be lost, as the
 * mechanism to return it is pthread-specific.
 *
 * @param value_ptr     Exit value.
 */
void pthread_exit(void *value_ptr) {
    if (pthread_self_pointer)
        pthread_self_pointer->exit_value = value_ptr;

    kern_thread_exit(0);
}

/**
 * Wait for thread termination. The calling thread will be blocked until the
 * given thread has terminated.
 *
 * @param thread        Thread to join with.
 * @param _value_ptr    Where to store thread's exit value.
 *
 * @return              0 on success.
 */
int pthread_join(pthread_t thread, void **_value_ptr) {
    object_event_t event = {};
    event.handle = thread->handle;
    event.event  = THREAD_EVENT_DEATH;

    /* Not allowed to return EINTR. TODO: This can go when we have syscall
     * restarting. */
    while (true) {
        status_t ret = kern_object_wait(&event, 1, 0, -1);

        /* Failure for other reasons shouldn't really happen. */
        libsystem_assert(ret == STATUS_SUCCESS || ret == STATUS_INTERRUPTED);

        if (ret == STATUS_SUCCESS)
            break;
    }

    pthread_release(thread);
    return 0;
}

/**
 * Get the POSIX thread ID of the calling thread. Note that this is not the
 * thread's kernel ID, it is a handle assigned by libsystem and is meaningless
 * to other processes.
 *
 * @return              POSIX thread ID for the calling thread.
 */
pthread_t pthread_self(void) {
    if (!pthread_self_pointer) {
        /* We weren't created by pthread_create() so make a pthread for ourself. */
        pthread_self_pointer = malloc(sizeof(*pthread_self_pointer));
        libsystem_assert(pthread_self_pointer);

        atomic_store_explicit(&pthread_self_pointer->refcount, 1, memory_order_relaxed);

        status_t ret = kern_thread_open(THREAD_SELF, &pthread_self_pointer->handle);
        libsystem_assert(ret == STATUS_SUCCESS);
    }

    return pthread_self_pointer;
}
