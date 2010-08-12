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
 * @brief		Put string functions.
 */

#include <string.h>
#include "stdio_priv.h"

/** Write string to a stream.
 *
 * Writes the contents of a string into a file stream.
 *
 * @param s		String to write.
 * @param stream	Stream to write to.
 *
 * @return		0 on success, EOF on failure or EOF.
 */
int fputs(const char *restrict s, FILE *restrict stream) {
	if(fwrite(s, strlen(s), 1, stream) != 1) {
		return EOF;
	}

	return 0;
}

/** Write string to standard output.
 *
 * Writes the contents of a string to standard output.
 *
 * @param s		String to write.
 *
 * @return		0 on success, EOF on failure or EOF.
 */
int puts(const char *s) {
	if(fputs(s, stdout) != 0) {
		return EOF;
	}

	if(fputc('\n', stdout) != '\n') {
		return EOF;
	}

	return 0;
}
