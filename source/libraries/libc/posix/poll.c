/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
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

#include "posix_priv.h"

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

		switch(object_type(fds[i].fd)) {
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

	ret = object_wait(events, count, (timeout < 0) ? -1 : (timeout * 1000));
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
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
