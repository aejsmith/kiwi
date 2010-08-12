/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		File open function.
 */

#include <kernel/object.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stdio_priv.h"

/** Standard input/output streams. */
FILE *stdin, *stdout, *stderr;

/** Internal part of fopen() and freopen().
 * @param path		Path to open.
 * @param mode		Mode string.
 * @param handlep	Where to store handle to file.
 * @return		New file descriptor or -1 on failure. */
static int fopen_internal(const char *restrict path, const char *restrict mode) {
	int flags;

	if(strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
		flags = O_RDONLY;
	} else if(strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
		flags = O_WRONLY | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) {
		flags = O_WRONLY | O_CREAT | O_APPEND;
	} else if(strcmp(mode, "r+") == 0 || strcmp(mode, "r+b") == 0 || strcmp(mode, "rb+") == 0) {
		flags = O_RDWR;
	} else if(strcmp(mode, "w+") == 0 || strcmp(mode, "w+b") == 0 || strcmp(mode, "rb+") == 0) {
		flags = O_RDWR | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a+") == 0 || strcmp(mode, "a+b") == 0 || strcmp(mode, "ab+") == 0) {
		flags = O_RDWR | O_CREAT | O_APPEND;
	} else {
		errno = EINVAL;
		return -1;
	}

	return open(path, flags, 0644);
}

/** Open file stream.
 *
 * Opens a new file stream for the file specified. The mode string specifies
 * the behaviour of the file stream. It can be any of the following:
 *  - r  - Open file for reading. Stream is positioned at start of file.
 *  - r+ - Open file for reading and writing. Stream is positioned at start of
 *         file.
 *  - w  - Truncate file to zero length or create file for writing. Stream is
 *         positioned at start of file.
 *  - w+ - Open file for reading and writing. Is created if it doesn't exist,
 *         otherwise is truncated. Stream is positioned at start of file.
 *  - a  - Open file for appending. Is created if it doesn't exist. Stream is
 *         positioned at end of the file.
 *  - a+ - Open file for reading and appending. Is created if it doesn't
 *         exist. Initial file position for reading is at start of file, but
 *         output is always appended to the end.
 * The mode string can also contain the character 'b', but this is ignored
 * and only retained for compatibility reasons.
 *
 * @param path		Path of file to open.
 * @param mode		Access mode string as described above.
 *
 * @return		Pointer to stream on success, NULL on failure.
 */
FILE *fopen(const char *restrict path, const char *restrict mode) {
	FILE *stream;

	stream = malloc(sizeof(FILE));
	if(!stream) {
		return NULL;
	}

	stream->fd = fopen_internal(path, mode);
	if(stream->fd < 0) {
		free(stream);
		return NULL;
	}

	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}

/** Create file stream from file descriptor
 *
 * Creates a new file stream referring to an existing file descriptor. The
 * given mode string should match the access flags of the file descriptor.
 *
 * @param fd		File descriptor to create for.
 * @param mode		Access mode string as described for fopen().
 *
 * @return		Pointer to stream on success, NULL on failure.
 */
FILE *fdopen(int fd, const char *mode) {
	FILE *stream;

	/* Check if the file descriptor is valid. */
	switch(object_type(fd)) {
	case OBJECT_TYPE_FILE:
	case OBJECT_TYPE_DEVICE:
		break;
	default:
		errno = EBADF;
		return NULL;
	}

	stream = malloc(sizeof(FILE));
	if(!stream) {
		return NULL;
	}

	stream->fd = fd;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}

/** Open file stream.
 *
 * Opens a new file stream for the file specified. The mode string specifies
 * the behaviour of the file stream. It can be any of the following:
 *  - r  - Open file for reading. Stream is positioned at start of file.
 *  - r+ - Open file for reading and writing. Stream is positioned at start of
 *         file.
 *  - w  - Truncate file to zero length or create file for writing. Stream is
 *         positioned at start of file.
 *  - w+ - Open file for reading and writing. Is created if it doesn't exist,
 *         otherwise is truncated. Stream is positioned at start of file.
 *  - a  - Open file for appending. Is created if it doesn't exist. Stream is
 *         positioned at end of the file.
 *  - a+ - Open file for reading and appending. Is created if it doesn't
 *         exist. Initial file position for reading is at start of file, but
 *         output is always appended to the end.
 * The mode string can also contain the character 'b', but this is ignored
 * and only retained for compatibility reasons. Rather than a new stream
 * being created, the given existing stream will be used (and any existing
 * stream closed if required).
 *
 * @param path		Path of file to open.
 * @param mode		Access mode string as described above.
 * @param stream	Pointer to stream structure to reuse.
 *
 * @return		Pointer to stream on success, NULL on failure. The
 *			original stream will not be changed on failure.
 */
FILE *freopen(const char *restrict path, const char *restrict mode, FILE *stream) {
	int fd;

	fd = fopen_internal(path, mode);
	if(fd < 0) {
		free(stream);
		return NULL;
	}

	close(stream->fd);
	stream->fd = fd;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}
