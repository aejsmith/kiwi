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
		libc_stub("setvbuf");
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}
