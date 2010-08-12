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
 * @brief		Get character functions.
 */

#include <unistd.h>
#include "stdio_priv.h"

/** Read character from a stream.
 * @param stream	Stream to read from.
 * @return		Chararcter read or EOF on failure. */
int fgetc(FILE *stream) {
	unsigned char ch;
	ssize_t ret;

	if(stream->have_pushback) {
		stream->have_pushback = false;
		return stream->pushback_ch;
	}

	ret = read(stream->fd, &ch, 1);
	if(ret < 0) {
		stream->err = 1;
		return EOF;
	} else if(ret < 1) {
		stream->eof = 1;
		return EOF;
	}

	return (int)ch;
}

/** Read character from a stream.
 * @param stream	Stream to read from.
 * @return		Chararcter read or EOF on failure. */
int getc(FILE *stream) {
	return fgetc(stream);
}

/** Read character from standard input.
 * @return		Chararcter read or EOF on failure. */
int getchar(void) {
	return fgetc(stdin);
}

/** Push a character back to a stream.
 *
 * Pushes the given character back onto the given input stream, to be read
 * by the next call to fgetc() or fread(). Only one character is stored: this
 * function will overwrite any existing pushed-back character.
 *
 * @param ch		Character to push.
 * @param stream	Stream to push to.
 *
 * @return		Character pushed.
 */
int ungetc(int ch, FILE *stream) {
	stream->pushback_ch = ch;
	stream->have_pushback = true;
	stream->eof = false;
	return ch;
}
