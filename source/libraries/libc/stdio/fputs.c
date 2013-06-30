/*
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
