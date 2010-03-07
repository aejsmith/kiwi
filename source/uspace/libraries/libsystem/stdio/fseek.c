/*
 * Copyright (C) 2008-2009 Alex Smith
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

#include <kernel/fs.h>

#include <stdio.h>
#include <string.h>

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
int fseek(FILE *stream, long off, int act) {
	switch(stream->type) {
	case STREAM_TYPE_FILE:
		if(fs_handle_seek(stream->handle, act, (offset_t)off, NULL) != 0) {
			return -1;
		}
		return 0;
	default:
		return 0;
	}
}

/** Set file pointer to beginning.
 *
 * Repositions the file pointer of a file stream to the beginning of the
 * file.
 *
 * @param stream	Stream to reposition.
 */
void rewind(FILE *stream) {
	fseek(stream, 0, FS_SEEK_SET);
	clearerr(stream);
}

/** Get file pointer.
 *
 * Gets the file pointer for a file stream.
 *
 * @param stream	Stream to reposition.
 *
 * @return		File pointer on success, -1 on failure.
 */
long ftell(FILE *stream) {
	offset_t new;

	switch(stream->type) {
	case STREAM_TYPE_FILE:
		if(fs_handle_seek(stream->handle, FS_SEEK_ADD, 0, &new) != 0) {
			return -1;
		}
		return new;
	default:
		return 0;
	}
}
