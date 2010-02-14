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
 * @brief		Get character functions.
 */

#include <kernel/device.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <stdlib.h>

#include "stdio_priv.h"

/** Read character from a stream.
 *
 * Reads the next character from a file stream and returns it.
 *
 * @param stream	Stream to read from.
 *
 * @return		Chararcter read or EOF on failure.
 */
int fgetc(FILE *stream) {
	unsigned char ch;
	size_t bytes;

	if(stream->have_pushback) {
		stream->have_pushback = false;
		return stream->pushback_ch;
	}

	switch(stream->type) {
	case STREAM_TYPE_FILE:
		if(fs_file_read(stream->handle, &ch, 1, -1, &bytes) != 0) {
			stream->err = true;
			return EOF;
		} else if(bytes != 1) {
			stream->eof = true;
			return EOF;
		}
		break;
	case STREAM_TYPE_DEVICE:
		if(device_read(stream->handle, &ch, 1, 0, &bytes) != 0) {
			stream->err = true;
			return EOF;
		} else if(bytes != 1) {
			stream->eof = true;
			return EOF;
		}
		break;
	default:
		stream->err = 1;
		return EOF;
	}

	return (int)ch;
}

/** Read character from a stream.
 *
 * Reads the next character from a file stream and returns it.
 *
 * @param stream	Stream to read from.
 *
 * @return		Chararcter read or EOF on failure.
 */
int getc(FILE *stream) {
	return fgetc(stream);
}

/** Read character from standard input.
 *
 * Reads the next character from standard input and returns it.
 *
 * @return		Chararcter read or EOF on failure.
 */
int getchar(void) {
	return fgetc(stdin);
}

/** Push a character back to a stream.
 *
 * Pushes the given character back onto the given input stream, to be read
 * by the next call to fgetc(). Only one character is stored: this function
 * will overwrite any existing pushed-back character.
 *
 * @param ch		Character to push.
 * @param stream	Stream to push to.
 *
 * @return		Character pushed.
 */
int ungetc(int ch, FILE *stream) {
	stream->pushback_ch = ch;
	stream->have_pushback = true;
	stream->eof = 0;
	return ch;
}
