/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Input/output multiplexing.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <poll.h>
#include <stdlib.h>

#include "posix/posix.h"

/** Information used to update poll table from info returned by kernel. */
typedef struct poll_convert {
	int index;			/**< Index of poll table entry. */
	short event;			/**< Value to OR into revents. */
} poll_convert_t;

/** Add an event to the kernel events array.
 * @param eventsp	Current events array.
 * @param convertp	Array of conversion information.
 * @param countp	Current size of array.
 * @param handle	Handle to add.
 * @param event		Event to wait for on handle.
 * @param pollidx	Index of entry in poll table.
 * @param pollev	Event to map back to.
 * @return		Whether successful. */
static bool add_event(object_event_t **eventsp, poll_convert_t **convertp, size_t *countp,
                      handle_t handle, int event, int pollidx, short pollev) {
	poll_convert_t *convert;
	object_event_t *events;
	size_t count = *countp;

	events = realloc(*eventsp, sizeof(*events) * (count + 1));
	if(!events) {
		return false;
	}
	*eventsp = events;

	convert = realloc(*convertp, sizeof(*convert) * (count + 1));
	if(!convert) {
		return false;
	}
	*convertp = convert;

	events[count].handle = handle;
	events[count].event = event;
	events[count].signalled = false;
	convert[count].index = pollidx;
	convert[count].event = pollev;
	*countp = count + 1;
	return true;
}

/** Wait for events on objects.
 * @param fds		Information on events to wait for.
 * @param nfds		Size of file descriptor array.
 * @param timeout	If negative, the function will block indefinitely until
 *			one of the events occurs. Otherwise, the number of
 *			milliseconds to wait for.
 * @return		Total number of file descriptors with returned events
 *			on success, -1 on failure. */
int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	poll_convert_t *convert = NULL;
	object_event_t *events = NULL;
	nfds_t signalled = 0;
	size_t count = 0, i;
	status_t ret;

	for(i = 0; i < nfds; i++) {
		fds[i].revents = 0;

		switch(kern_object_type(fds[i].fd)) {
		case OBJECT_TYPE_FILE:
			if(fds[i].events & ~(POLLIN | POLLOUT)) {
				errno = ENOTSUP;
				goto fail;
			}

			/* Quote: "Regular files shall always poll TRUE for
			 * reading and writing". */
			if(fds[i].events & POLLIN) {
				fds[i].revents |= POLLIN;
				signalled++;
			}
			if(fds[i].events & POLLOUT) {
				fds[i].revents |= POLLOUT;
				signalled++;
			}
			break;
		case OBJECT_TYPE_DEVICE:
			if(fds[i].events & ~(POLLIN | POLLOUT)) {
				errno = ENOTSUP;
				goto fail;
			}

			if(fds[i].events & POLLIN) {
				if(!add_event(&events, &convert, &count, fds[i].fd,
				              DEVICE_EVENT_READABLE, i, POLLIN)) {
					goto fail;
				}
			}
			if(fds[i].events & POLLOUT) {
				if(!add_event(&events, &convert, &count, fds[i].fd,
				              DEVICE_EVENT_WRITABLE, i, POLLOUT)) {
					goto fail;
				}
			}
			break;
		case -1:
			fds[i].revents = POLLNVAL;
			signalled++;
			break;
		default:
			errno = ENOTSUP;
			goto fail;
		}
	}

	/* Check if we need to do anything more. */
	if(signalled) {
		goto out;
	} else if(!count) {
		errno = EINVAL;
		return -1;
	}

	ret = kern_object_wait(events, count, (timeout < 0) ? -1 : (timeout * 1000));
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		goto fail;
	}

	/* Update the poll table from what the kernel returned. */
	for(i = 0; i < count; i++) {
		if(events[i].signalled) {
			fds[convert[i].index].revents |= convert[i].event;
			signalled++;
		}
	}
out:
	free(events);
	free(convert);
	return signalled;
fail:
	free(events);
	free(convert);
	return -1;
}
