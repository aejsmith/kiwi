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
 * @brief		Terminal device manager.
 */

#ifndef __TTY_PRIV_H
#define __TTY_PRIV_H

#include <io/device.h>

#include <ipc/pipe.h>

#include <mm/page.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include "../../../include/termios.h"

/** Terminal buffer size. */
#define TTY_BUFFER_SIZE		(PAGE_SIZE / sizeof(uint16_t))

/** Special flags for characters in a terminal buffer. */
#define TTY_CHAR_ESCAPED	(1<<8)	/**< Character is escaped. */
#define TTY_CHAR_NEWLINE	(1<<9)	/**< Character is classed as a new line. */
#define TTY_CHAR_EOF		(1<<10)	/**< Character is an end-of-file. */

/** Terminal input buffer. */
typedef struct tty_buffer {
	mutex_t lock;			/**< Lock to protect buffer. */

	semaphore_t lines;		/**< Semaphore for line-oriented reads. */
	notifier_t lines_notifier;	/**< Notifier for line availability. */
	semaphore_t data;		/**< Semaphore for character-oriented reads. */
	notifier_t data_notifier;	/**< Notifier for data availability. */
	semaphore_t space;		/**< Queue to wait for space to write to. */
	notifier_t space_notifier;	/**< Notifier for space availability. */

	size_t start;			/**< Start position of buffer. */
	size_t end;			/**< End position of buffer. */
	uint16_t *buffer;		/**< Circular data buffer. */
} tty_buffer_t;

/** Terminal device data structure. */
typedef struct tty_device {
	mutex_t lock;			/**< Lock for the terminal. */
	int id;				/**< ID of terminal. */
	refcount_t count;		/**< References to the terminal. */
	pipe_t *output;			/**< Buffer containing output from slave. */
	tty_buffer_t *input;		/**< Buffer contaning input for slave. */
	device_t *slave;		/**< Slave device. */
	bool escaped;			/**< Whether the next input character is escaped. */
	bool inhibited;			/**< Whether output has been stopped. */
	struct termios termios;		/**< Terminal I/O settings. */
	struct winsize winsize;		/**< Window size. */
} tty_device_t;

extern status_t tty_buffer_read_line(tty_buffer_t *buf, char *dest, size_t count,
                                     bool nonblock, size_t *bytesp);
extern status_t tty_buffer_read(tty_buffer_t *buf, char *dest, size_t count, bool nonblock,
                                size_t *bytesp);
extern bool tty_buffer_erase(tty_buffer_t *buf);
extern size_t tty_buffer_kill(tty_buffer_t *buf);
extern status_t tty_buffer_insert(tty_buffer_t *buf, uint16_t ch, bool nonblock);
extern tty_buffer_t *tty_buffer_create(void);
extern void tty_buffer_destroy(tty_buffer_t *buf);

#endif /* __TTY_PRIV_H */
