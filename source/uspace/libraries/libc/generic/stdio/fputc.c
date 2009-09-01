/* Kiwi C library - Put character functions
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

#include <stdio.h>
#include <stdlib.h>

#include "stdio_priv.h"

#if 0
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
	ssize_t ret;

	ret = write(stream->fd, &val, 1);
	if(ret < 0) {
		stream->err = 1;
		return EOF;
	} else if(ret < 1) {
		stream->eof = 1;
		return EOF;
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
#endif

extern void putch(char ch);

/** Write character to standard output.
 *
 * Writes a character to standard output.
 *
 * @param ch		Character to write.
 *
 * @return		EOF on failure, character written on success.
 */
int putchar(int ch) {
	//return fputc(ch, stdout);
	putch((char)ch);
	return ch;
}
