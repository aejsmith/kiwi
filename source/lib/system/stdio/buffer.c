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
 * @brief		File buffer functions.
 */

#include <errno.h>
#include "stdio_priv.h"

/** Set a stream's buffering mode.
 * @param stream	Stream to set mode for.
 * @param buf		If not NULL, the stream will be set to fully buffered
 *			mode with buf as the buffer. If NULL, buffering will
 *			be disabled. */
void setbuf(FILE *restrict stream, char *restrict buf) {
	setvbuf(stream, buf, (buf) ? _IOFBF : _IONBF, BUFSIZ);
}

/** Set a stream's buffering mode.
 * @param stream	Stream to set mode for.
 * @param buf		If not NULL, a preallocated buffer to use.
 * @param mode		Buffering mode.
 * @param size		Size of the provided buffer.
 * @return		0 on success, -1 on failure with errno set. */
int setvbuf(FILE *restrict stream, char *restrict buf, int mode, size_t size) {
	switch(mode) {
	case _IONBF:
		break;
	case _IOLBF:
	case _IOFBF:
		libsystem_stub("setvbuf", true);
		return -1;
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}
