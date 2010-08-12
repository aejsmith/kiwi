/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		File seek functions.
 */

#include <unistd.h>
#include "stdio_priv.h"

/** Reposition file pointer.
 *
 * Repositions the file pointer for a file stream according to the action
 * specified.
 *
 * @param stream	Stream to reposition.
 * @param off		New offset.
 * @param act		How to set the offset.
 *
 * @return		0 on success, -1 on failure.
 */
int fseeko(FILE *stream, off_t off, int act) {
	off_t ret;

	ret = lseek(stream->fd, off, act);
	if(ret < 0) {
		return -1;
	}

	return 0;
}

/** Reposition file pointer.
 *
 * Repositions the file pointer for a file stream according to the action
 * specified.
 *
 * @param stream	Stream to reposition.
 * @param off		New offset.
 * @param act		How to set the offset.
 *
 * @return		0 on success, -1 on failure.
 */
int fseek(FILE *stream, long off, int act) {
	return fseeko(stream, (off_t)off, act);
}

/** Set file pointer to beginning of file
 * @param stream	Stream to reposition. */
void rewind(FILE *stream) {
	fseek(stream, 0, SEEK_SET);
	clearerr(stream);
}

/** Get file pointer.
 * @param stream	Stream to get pointer for.
 * @return		File pointer on success, -1 on failure. */
off_t ftello(FILE *stream) {
	return lseek(stream->fd, 0, SEEK_CUR);
}

/** Get file pointer.
 * @param stream	Stream to get pointer for.
 * @return		File pointer on success, -1 on failure. */
long ftell(FILE *stream) {
	return (long)lseek(stream->fd, 0, SEEK_CUR);
}
