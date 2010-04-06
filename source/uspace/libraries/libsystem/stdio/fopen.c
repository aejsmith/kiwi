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
 * @brief		File open function.
 */

#include <kernel/device.h>
#include <kernel/errors.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio_priv.h"

/** Internal part of fopen() and freopen() for files.
 * @param path		Path to open.
 * @param mode		Mode string.
 * @return		Handle on success, negative error code on failure. */
static int fopen_file_internal(const char *path, const char *mode) {
	int flags;

	if(strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
		flags = FS_FILE_READ;
	} else if(strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
		/* FIXME: Create/truncate. */
		flags = FS_FILE_WRITE;
	} else if(strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) {
		/* FIXME: Create. */
		flags = FS_FILE_WRITE | FS_FILE_APPEND;
	} else if(strcmp(mode, "r+") == 0 || strcmp(mode, "r+b") == 0 || strcmp(mode, "rb+") == 0) {
		flags = FS_FILE_READ | FS_FILE_WRITE;
	} else if(strcmp(mode, "w+") == 0 || strcmp(mode, "w+b") == 0 || strcmp(mode, "rb+") == 0) {
		/* FIXME: Create/truncate. */
		flags = FS_FILE_READ | FS_FILE_WRITE;
	} else if(strcmp(mode, "a+") == 0 || strcmp(mode, "a+b") == 0 || strcmp(mode, "ab+") == 0) {
		flags = FS_FILE_READ | FS_FILE_WRITE | FS_FILE_APPEND;
	} else {
		return -ERR_PARAM_INVAL;
	}

	return fs_file_open(path, flags);
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
FILE *fopen(const char *path, const char *mode) {
	FILE *stream;

	if(!(stream = malloc(sizeof(FILE)))) {
		return NULL;
	} else if((stream->handle = fopen_file_internal(path, mode)) < 0) {
		free(stream);
		return NULL;
	}

	stream->type = STREAM_TYPE_FILE;
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
FILE *freopen(const char *path, const char *mode, FILE *stream) {
	handle_id_t handle;

	if((handle = fopen_file_internal(path, mode)) < 0) {
		return NULL;
	} else if(fclose_internal(stream) != 0) {
		return NULL;
	}

	stream->type = STREAM_TYPE_FILE;
	stream->handle = handle;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}

/** Create a file stream from an existing handle.
 * @param handle	Handle to open.
 * @param stream	If not NULL, this structure will be used rather than
 *			allocating a new one.
 * @return		Pointer to stream on success, NULL on failure. */
FILE *fopen_handle(handle_id_t handle, FILE *stream) {
	int type;

	/* Check if the handle can be used. */
	type = object_type(handle);
	if(type != OBJECT_TYPE_FILE && type != OBJECT_TYPE_DEVICE) {
		return NULL;
	}

	if(!stream && !(stream = malloc(sizeof(FILE)))) {
		return NULL;
	}

	stream->type = (type == OBJECT_TYPE_DEVICE) ? STREAM_TYPE_DEVICE : OBJECT_TYPE_FILE;
	stream->handle = handle;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}

/** Open a device stream.
 * @param path		Device path to open.
 * @param stream	If not NULL, this structure will be used rather than
 *			allocating a new one.
 * @return		Pointer to stream on success, NULL on failure. */
FILE *fopen_device(const char *path, FILE *stream) {
	handle_id_t handle;

	if((handle = device_open(path)) < 0) {
		return NULL;
	} else if(!stream && !(stream = malloc(sizeof(FILE)))) {
		handle_close(handle);
		return NULL;
	}

	stream->type = STREAM_TYPE_DEVICE;
	stream->handle = handle;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}

/** Open a stream to the kernel console.
 * @param stream	If not NULL, this structure will be used rather than
 *			allocating a new one.
 * @return		Pointer to stream on success, NULL on failure. */
FILE *fopen_kconsole(FILE *stream) {
	if(!stream && !(stream = malloc(sizeof(FILE)))) {
		return NULL;
	}

	stream->type = STREAM_TYPE_KCONSOLE;
	stream->err = false;
	stream->eof = false;
	stream->have_pushback = false;
	return stream;
}
