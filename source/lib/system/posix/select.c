/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Synchronous I/O multiplexing.
 */

#include <core/time.h>

#include <kernel/object.h>
#include <kernel/file.h>
#include <kernel/status.h>

#include <sys/select.h>

#include <errno.h>
#include <stdlib.h>

#include "libsystem.h"

static bool add_event(object_event_t **_events, size_t *_count, handle_t handle, unsigned event) {
    size_t index = *_count;
    size_t count = index + 1;

    object_event_t *events = realloc(*_events, sizeof(*events) * count);
    if (!events)
        return false;

    *_events = events;
    *_count  = count;

    events[index].handle = handle;
    events[index].event  = event;
    events[index].flags  = 0;
    events[index].data   = 0;
    events[index].udata  = NULL;

    return true;
}

/** Wait for file descriptors to be ready. */
int pselect(
    int nfds, fd_set *__restrict readfds, fd_set *__restrict writefds,
    fd_set *__restrict errorfds, const struct timespec *__restrict timeout,
    const sigset_t *__restrict sigmask)
{
    status_t ret;

    if (nfds > FD_SETSIZE)
        return EINVAL;

    object_event_t *events __sys_cleanup_free = NULL;
    size_t count = 0;

    for (int i = 0; i < nfds; i++) {
        handle_t handle = i;

        bool is_read  = readfds  && FD_ISSET(i, readfds);
        bool is_write = writefds && FD_ISSET(i, writefds);

        /* We don't implement error conditions. */
        if (!is_read && !is_write)
            continue;

        unsigned type;
        ret = kern_object_type(handle, &type);
        if (ret != STATUS_SUCCESS || type != OBJECT_TYPE_FILE) {
            errno = EBADF;
            return -1;
        }

        if (is_read) {
            if (!add_event(&events, &count, handle, FILE_EVENT_READABLE))
                return -1;
        }

        if (is_write) {
            if (!add_event(&events, &count, handle, FILE_EVENT_WRITABLE))
                return -1;
        }
    }

    if (count > 0) {
        /*
        * TODO: We're not handling signals here properly. First, this should
        * affect the current thread signal mask once we support pthread signals.
        * Second, the behaviour of this is supposed to be atomic - if setting
        * the new mask allows a signal to be delivered, it should cause this
        * function to return EINTR. However, we'll handle the signal and then
        * continue to call kern_object_wait().
        */
        sigset_t orig_mask;
        if (sigmask) {
            if (sigprocmask(SIG_SETMASK, sigmask, &orig_mask) != 0)
                return -1;
        }

        nstime_t nsecs = (timeout)
            ? core_secs_to_nsecs(timeout->tv_sec) + timeout->tv_nsec
            : -1;

        ret = kern_object_wait(events, count, 0, nsecs);

        if (sigmask)
            sigprocmask(SIG_SETMASK, &orig_mask, NULL);

        if (ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK) {
            /* If these are returned, no events should have been returned. */
        } else if (ret != STATUS_SUCCESS) {
            libsystem_status_to_errno(ret);
            return -1;
        }
    }

    if (readfds)    FD_ZERO(readfds);
    if (writefds)   FD_ZERO(writefds);
    if (errorfds)   FD_ZERO(errorfds);

    int num_set = 0;

    for (size_t i = 0; i < count; i++) {
        if (events[i].flags & OBJECT_EVENT_SIGNALLED) {
            if (events[i].event == FILE_EVENT_READABLE) {
                FD_SET(events[i].handle, readfds);
                num_set++;
            } else if (events[i].event == FILE_EVENT_WRITABLE) {
                FD_SET(events[i].handle, writefds);
                num_set++;
            }
        }
    }

    return num_set;
}

/** Wait for file descriptors to be ready. */
int select(
    int nfds, fd_set *__restrict readfds, fd_set *__restrict writefds,
    fd_set *__restrict errorfds, struct timeval *__restrict timeout)
{
    struct timespec spec = {};
    if (timeout) {
        spec.tv_sec  = timeout->tv_sec;
        spec.tv_nsec = core_usecs_to_nsecs(timeout->tv_usec);
    }

    /* POSIX allows timeout to be modified to the time that was left, and Linux
     * does this, but we do not for now. */
    return pselect(nfds, readfds, writefds, errorfds, (timeout) ? &spec : NULL, NULL);
}
