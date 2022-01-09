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
                if (fds[i].events & ~(POLLIN | POLLOUT)) {
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

    /* INVALID_EVENT would signal ERROR on the corresponding handle. */
    if (ret != STATUS_SUCCESS && ret != STATUS_INVALID_EVENT) {
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
