/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Unidirectional data pipe implementation.
 */

#ifndef __IPC_PIPE_H
#define __IPC_PIPE_H

#include <lib/notifier.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

/** Size of a pipe's data buffer. */
#define PIPE_SIZE	4096

/** Structure containing a pipe. */
typedef struct pipe {
	mutex_t lock;			/**< Lock to protect buffer. */

	semaphore_t space_sem;		/**< Semaphore counting available space. */
	notifier_t space_notifier;	/**< Notifier for space availability. */
	semaphore_t data_sem;		/**< Semaphore counting available data. */
	notifier_t data_notifier;	/**< Notifier for data availability. */

	char *buf;			/**< Circular data buffer. */
	size_t start;			/**< Start position of buffer. */
	size_t end;			/**< End position of buffer. */
} pipe_t;

extern status_t pipe_read(pipe_t *pipe, char *buf, size_t count, bool nonblock, size_t *bytesp);
extern status_t pipe_write(pipe_t *pipe, const char *buf, size_t count, bool nonblock, size_t *bytesp);
extern void pipe_wait(pipe_t *pipe, bool write, void *sync);
extern void pipe_unwait(pipe_t *pipe, bool write, void *sync);

extern pipe_t *pipe_create(void);
extern void pipe_destroy(pipe_t *pipe);

#endif /* __IPC_PIPE_H */
