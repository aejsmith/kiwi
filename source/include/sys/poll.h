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

#ifndef __SYS_POLL_H
#define __SYS_POLL_H

#define __need_size_t
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Poll file descriptor information. */
struct pollfd {
	int fd;				/**< File descriptor being polled. */
	short events;			/**< Events to wait for. */
	short revents;			/**< Events returned from the poll() call. */
};

/** Type used to store a number of file descriptors. */
typedef size_t nfds_t;

/** Events for poll(). */
#define POLLIN			(1<<0)	/**< Normal data may be read without blocking. */
#define POLLPRI			(1<<1)	/**< High-priority data may be read without blocking. */
#define POLLRDBAND		(1<<2)	/**< Priority data may be read without blocking. */
#define POLLOUT			(1<<3)	/**< Normal data may be written without blocking. */
#define POLLWRBAND		(1<<4)	/**< Priority data may be written without blocking. */
#define POLLERR			(1<<5)	/**< An error has occurred (output only). */
#define POLLHUP			(1<<6)	/**< Device has been disconnected (output only). */
#define POLLNVAL		(1<<7)	/**< Invalid file descriptor (output only). */
#define POLLRDNORM		POLLIN	/**< Equivalent to POLLIN. */
#define POLLWRNORM		POLLOUT	/**< Equivalent to POLLOUT. */

extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_POLL_H */
