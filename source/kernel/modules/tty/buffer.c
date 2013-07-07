/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Terminal input buffer.
 */

#include <lib/string.h>

#include <mm/kmem.h>
#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

#include "tty_priv.h"

/** Get a character from a terminal buffer.
 * @param buf		Buffer to read from.
 * @return		Character read. */
static inline uint16_t tty_buffer_get(tty_buffer_t *buf) {
	uint16_t ch = buf->buffer[buf->start];
	if(++buf->start >= TTY_BUFFER_SIZE) {
		buf->start = 0;
	}

	semaphore_up(&buf->space, 1);
	notifier_run(&buf->space_notifier, NULL, false);
	return ch;
}

/**
 * Read a line from a terminal buffer.
 *
 * Reads up to the given number of bytes from a terminal buffer, but at most
 * 1 line.
 *
 * @param buf		Terminal buffer to read from.
 * @param dest		Destination buffer.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		Status code describing result of the operation.
 */
status_t tty_buffer_read_line(tty_buffer_t *buf, char *dest, size_t count, bool nonblock,
                              size_t *bytesp) {
	status_t ret;
	uint16_t ch;
	size_t i;

	/* Wait for a line to come into the buffer. */
	ret = semaphore_down_etc(&buf->lines, (nonblock) ? 0 : -1, SYNC_INTERRUPTIBLE);
	if(ret != STATUS_SUCCESS) {
		*bytesp = 0;
		return ret;
	}

	mutex_lock(&buf->lock);

	/* Read at most the number of bytes necessary. */
	for(i = 0; i < count; i++) {
		/* If we have a line, there must be data. */
		ret = semaphore_down_etc(&buf->data, 0, 0);
		assert(ret == STATUS_SUCCESS);

		ch = tty_buffer_get(buf);
		dest[i] = ch & 0xFF;

		/* Check if this is the end of the line. */
		if(ch & TTY_CHAR_NEWLINE) {
			/* An EOF character should not increase the number of
			 * bytes read. */
			if(!(ch & TTY_CHAR_EOF)) {
				i++;
			}

			goto out;
		}
	}

	/* If we get here, we haven't read the newline that occurred. Put the
	 * count back up. */
	semaphore_up(&buf->lines, 1);
	notifier_run(&buf->lines_notifier, NULL, false);
out:
	mutex_unlock(&buf->lock);
	*bytesp = i;
	return STATUS_SUCCESS;
}

/** Read from a terminal buffer.
 * @param buf		Terminal buffer to read from.
 * @param dest		Destination buffer.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes read
 * @return		Status code describing result of the operation. */
status_t tty_buffer_read(tty_buffer_t *buf, char *dest, size_t count, bool nonblock, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	uint16_t ch;
	size_t i;

	for(i = 0; i < count; i++) {
		ret = semaphore_down_etc(&buf->data, (nonblock) ? 0 : -1, SYNC_INTERRUPTIBLE);
		if(ret != STATUS_SUCCESS) {
			break;
		}

		mutex_lock(&buf->lock);

		ch = tty_buffer_get(buf);
		dest[i] = ch & 0xFF;

		/* Keep the line count consistent. */
		if(ch & TTY_CHAR_NEWLINE) {
			ret = semaphore_down_etc(&buf->lines, 0, 0);
			assert(ret == STATUS_SUCCESS);
		}

		mutex_unlock(&buf->lock);
	}

	*bytesp = i;
	return ret;
}

/** Erase the last character from a terminal buffer.
 * @param buf		Buffer to erase from.
 * @return		Whether a character was erased. */
static bool tty_buffer_erase_unsafe(tty_buffer_t *buf) {
	status_t ret;
	size_t last;

	last = (buf->end > 0) ? (buf->end - 1) : (TTY_BUFFER_SIZE - 1);
	if(buf->start == buf->end || buf->buffer[last] & TTY_CHAR_NEWLINE) {
		return false;
	}

	ret = semaphore_down_etc(&buf->data, 0, 0);
	assert(ret == STATUS_SUCCESS);

	buf->end = last;
	semaphore_up(&buf->space, 1);
	notifier_run(&buf->space_notifier, NULL, false);
	return true;
}

/** Erase a character from a terminal buffer, unless at start of line.
 * @param buf		Buffer to erase from.
 * @return		Whether a character was erased. */
bool tty_buffer_erase(tty_buffer_t *buf) {
	bool ret;

	mutex_lock(&buf->lock);
	ret = tty_buffer_erase_unsafe(buf);
	mutex_unlock(&buf->lock);
	return ret;
}

/** Erase a line from a terminal buffer.
 * @param buf		Buffer to erase from.
 * @return		Number of characters killed. */
size_t tty_buffer_kill(tty_buffer_t *buf) {
	size_t ret = 0;

	mutex_lock(&buf->lock);

	while(tty_buffer_erase_unsafe(buf)) {
		ret++;
	}

	mutex_unlock(&buf->lock);
	return ret;
}

/** Insert a character into a terminal buffer.
 * @param buf		Buffer to insert to.
 * @param ch		Character to insert. Should be marked with the necessary
 *			TTY_CHAR_* flags.
 * @param nonblock	Whether to allow blocking.
 * @return		Status code describing result of the operation. */
status_t tty_buffer_insert(tty_buffer_t *buf, uint16_t ch, bool nonblock) {
	status_t ret;

	ret = semaphore_down_etc(&buf->space, (nonblock) ? 0 : -1, SYNC_INTERRUPTIBLE);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	mutex_lock(&buf->lock);

	buf->buffer[buf->end] = ch;
	if(++buf->end >= TTY_BUFFER_SIZE) {
		buf->end = 0;
	}

	if(ch & TTY_CHAR_NEWLINE) {
		semaphore_up(&buf->lines, 1);
		notifier_run(&buf->lines_notifier, NULL, false);
	}
	semaphore_up(&buf->data, 1);
	notifier_run(&buf->data_notifier, NULL, false);

	mutex_unlock(&buf->lock);
	return STATUS_SUCCESS;
}

/** Create a new terminal buffer.
 * @return		Pointer to created buffer. */
tty_buffer_t *tty_buffer_create(void) {
	tty_buffer_t *buf;

	buf = kmalloc(sizeof(tty_buffer_t), MM_KERNEL);
	mutex_init(&buf->lock, "tty_buffer_lock", 0);
	semaphore_init(&buf->lines, "tty_buffer_lines", 0);
	semaphore_init(&buf->data, "tty_buffer_data", 0);
	semaphore_init(&buf->space, "tty_buffer_space", TTY_BUFFER_SIZE);
	notifier_init(&buf->lines_notifier, buf);
	notifier_init(&buf->data_notifier, buf);
	notifier_init(&buf->space_notifier, buf);
	buf->start = 0;
	buf->end = 0;
	buf->buffer = kmem_alloc(TTY_BUFFER_SIZE * sizeof(*buf->buffer), MM_KERNEL);
	return buf;
}

/** Destroy a terminal buffer.
 * @param buf		Buffer to destroy. */
void tty_buffer_destroy(tty_buffer_t *buf) {
	kmem_free(buf->buffer, TTY_BUFFER_SIZE * sizeof(*buf->buffer));
	notifier_clear(&buf->lines_notifier);
	notifier_clear(&buf->data_notifier);
	notifier_clear(&buf->space_notifier);
	kfree(buf);
}
