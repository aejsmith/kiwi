/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Input/output multiplexing.
 */

#include <core/time.h>

#include <kernel/object.h>
#include <kernel/file.h>
#include <kernel/status.h>

#include <errno.h>
#include <poll.h>
#include <stdlib.h>

#include "libsystem.h"

/** Information used to update poll table from info returned by kernel. */
typedef struct poll_convert {
    size_t index;                   /**< Index of poll table entry. */
    short event;                    /**< Value to OR into revents. */
} poll_convert_t;

static bool add_event(
    object_event_t **_events, poll_convert_t **_convert, size_t *_count,
    handle_t handle, unsigned event, size_t poll_index, short poll_event)
{
    size_t index = *_count;
    size_t count = index + 1;

    object_event_t *events = realloc(*_events, sizeof(*events) * count);
    if (!events)
        return false;

    *_events = events;

    poll_convert_t *convert = realloc(*_convert, sizeof(*convert) * count);
    if (!convert)
        return false;

    *_convert = convert;

    events[index].handle = handle;
    events[index].event  = event;
    events[index].flags  = 0;
    events[index].data   = 0;
    events[index].udata  = NULL;

    convert[index].index = poll_index;
    convert[index].event = poll_event;

    *_count = count;
    return true;
}

/** Wait for events on objects.
 * @param fds           Information on events to wait for.
 * @param nfds          Size of file descriptor array.
 * @param timeout       If negative, the function will block indefinitely until
 *                      one of the events occurs. Otherwise, the number of
 *                      milliseconds to wait for.
 * @return              Total number of file descriptors with returned events
 *                      on success, -1 on failure. */
int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    status_t ret;

    for (nfds_t i = 0; i < nfds; i++)
        fds[i].revents = 0;

    poll_convert_t *convert __sys_cleanup_free = NULL;
    object_event_t *events __sys_cleanup_free = NULL;
    size_t count = 0;

    for (nfds_t i = 0; i < nfds; i++) {
        unsigned type;
        ret = kern_object_type(fds[i].fd, &type);
        if (ret != STATUS_SUCCESS) {
            fds[i].revents |= POLLNVAL;
            continue;
        }

        switch (type) {
            case OBJECT_TYPE_FILE:
                if (fds[i].events & ~(POLLIN | POLLOUT | POLLPRI | POLLHUP | POLLRDBAND)) {
                    errno = ENOTSUP;
                    return -1;
                }

                if (fds[i].events & POLLIN) {
                    if (!add_event(
                            &events, &convert, &count, fds[i].fd,
                            FILE_EVENT_READABLE, i, POLLIN))
                    {
                        return -1;
                    }
                }

                if (fds[i].events & POLLOUT) {
                    if (!add_event(
                            &events, &convert, &count, fds[i].fd,
                            FILE_EVENT_WRITABLE, i, POLLOUT))
                    {
                        return -1;
                    }
                }

                if (fds[i].events & POLLHUP) {
                    if (!add_event(
                            &events, &convert, &count, fds[i].fd,
                            FILE_EVENT_HANGUP, i, POLLHUP))
                    {
                        return -1;
                    }
                }

                // TODO: POLLPRI. This is probably only relevant for out of
                // band data on sockets.

                break;
            default:
                errno = ENOTSUP;
                return -1;
        }
    }

    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    ret = kern_object_wait(events, count, 0, (timeout < 0) ? -1 : core_msecs_to_nsecs(timeout));

    if (ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK) {
        /* If these are returned, no events should have been returned. */
    } else if (ret == STATUS_INVALID_EVENT) {
        /* INVALID_EVENT would signal ERROR on the corresponding handle. */
    } else if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    /* Update the poll table from what the kernel returned. */
    nfds_t signalled = 0;
    for (nfds_t i = 0; i < count; i++) {
        if (events[i].flags & OBJECT_EVENT_ERROR) {
            fds[convert[i].index].revents |= POLLERR;
            signalled++;
        } else if (events[i].flags & OBJECT_EVENT_SIGNALLED) {
            fds[convert[i].index].revents |= convert[i].event;
            signalled++;
        }
    }

    return signalled;
}
