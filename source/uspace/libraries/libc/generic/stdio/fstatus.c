/* Kiwi C library - File stream status functions
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
 * @brief		File stream status functions.
 */

#include "stdio_priv.h"

/** Clear stream error status.
 *
 * Clears the error and end-of-file status for a file stream.
 *
 * @param stream	Stream to clear.
 */
void clearerr(FILE *stream) {
	stream->eof = false;
	stream->err = false;
}

/** Get end-of-file status for file stream.
 *
 * Gets the end-of-file status value for a file stream.
 *
 * @param stream	Stream to get value from.
 *
 * @return		End-of-file status.
 */
int feof(FILE *stream) {
	return stream->eof;
}

/** Get error status for file stream.
 *
 * Gets the error status value for a file stream.
 *
 * @param stream	Stream to get value from.
 *
 * @return		Error status.
 */
int ferror(FILE *stream) {
	return stream->err;
}
