/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Unidirectional data pipe implementation.
 */

#ifndef __IPC_PIPE_H
#define __IPC_PIPE_H

#include <lib/notifier.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

/** Size of a pipe's data buffer. */
#define PIPE_SIZE	4096

struct object_wait;

/** Structure containing a pipe. */
typedef struct pipe {
	mutex_t reader;			/**< Lock to serialize read requests. */
	mutex_t writer;			/**< Lock to serialize write requests. */
	mutex_t lock;			/**< Lock to protect buffer. */

	semaphore_t space_sem;		/**< Semaphore counting available space. */
	notifier_t space_notifier;	/**< Notifier for space availability. */
	semaphore_t data_sem;		/**< Semaphore counting available data. */
	notifier_t data_notifier;	/**< Notifier for data availability. */

	char *buf;			/**< Circular data buffer. */
	size_t start;			/**< Start position of buffer. */
	size_t end;			/**< End position of buffer. */
} pipe_t;

extern int pipe_read(pipe_t *pipe, char *buf, size_t count, bool nonblock, size_t *bytesp);
extern int pipe_write(pipe_t *pipe, const char *buf, size_t count, bool nonblock, size_t *bytesp);
extern void pipe_wait(pipe_t *pipe, bool write, struct object_wait *wait);
extern void pipe_unwait(pipe_t *pipe, bool write, struct object_wait *wait);

extern pipe_t *pipe_create(void);
extern void pipe_destroy(pipe_t *pipe);

#endif /* __IPC_PIPE_H */
