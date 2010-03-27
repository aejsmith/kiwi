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
 * @brief		Put character functions.
 */

#include <kernel/device.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <stdlib.h>

#include "../libsystem.h"
#include "stdio_priv.h"

/** Write character to a stream.
 *
 * Writes a character to a file stream.
 *
 * @param ch		Character to write.
 * @param stream	Stream to write to.
 *
 * @return		EOF on failure, character written on success.
 */
int fputc(int ch, FILE *stream) {
	unsigned char val = (unsigned char)ch;
	size_t bytes;

	switch(stream->type) {
	case STREAM_TYPE_FILE:
		if(fs_file_write(stream->handle, &val, 1, &bytes) != 0) {
			stream->err = true;
			return EOF;
		} else if(bytes != 1) {
			stream->eof = true;
			return EOF;
		}
		break;
	case STREAM_TYPE_DEVICE:
		if(device_write(stream->handle, &val, 1, 0, &bytes) != 0) {
			stream->err = true;
			return EOF;
		} else if(bytes != 1) {
			stream->eof = true;
			return EOF;
		}
		break;
	case STREAM_TYPE_KCONSOLE:
		kputch(ch);
		break;
	}

	return (int)val;
}

/** Write character to a stream.
 *
 * Writes a character to a file stream.
 *
 * @param ch		Character to write.
 * @param stream	Stream to write to.
 *
 * @return		EOF on failure, character written on success.
 */
int putc(int ch, FILE *stream) {
	return fputc(ch, stream);
}

/** Write character to standard output.
 *
 * Writes a character to standard output.
 *
 * @param ch		Character to write.
 *
 * @return		EOF on failure, character written on success.
 */
int putchar(int ch) {
	return fputc(ch, stdout);
}
