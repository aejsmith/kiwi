/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Get string functions.
 */

#include "stdio/stdio.h"

/**
 * Read a string from standard input.
 *
 * Reads a string from standard input to a buffer. Use of this function is
 * not wise as it is not possible to tell whether a buffer-overrun occurs,
 * and therefore use of it imposes a security risk.
 *
 * @param s		Buffer to read into.
 *
 * @return		Buffer on success, NULL on failure or EOF.
 */
char *gets(char *s) {
	int i = 0, ch;

	while(1) {
		ch = fgetc(stdin);
		if(ch == EOF) {
			if(i > 0 && feof(stdin)) {
				s[i] = '\0';
				return s;
			} else {
				return NULL;
			}
		} else if(ch == '\n') {
			s[i] = '\0';
			return s;
		} else if(ch == '\b') {
			if(i)
				s[--i] = 0;
		} else {
			s[i] = ch;
		}

		i++;
	}
}

/** Read a string from a file stream.
 * @param s		Buffer to read into.
 * @param size		Maximum number of characters to read.
 * @param stream	Stream to read from.
 * @return		Buffer on success, NULL on failure or EOF. */
char *fgets(char *s, int size, FILE *stream) {
	int i, ch;

	for(i = 0; i < size - 1; i++) {
		ch = fgetc(stream);
		if(ch == EOF) {
			if(i > 0 && feof(stream)) {
				s[i] = '\0';
				return s;
			} else {
				return NULL;
			}
		} else if(ch == '\n') {
			s[i] = '\n';
			s[i + 1] = '\0';

			return s;
		} else if(ch == '\b') {
			if(i) {
				s[--i] = 0;
				i--;
			}
		} else {
			s[i] = ch;
		}
	}

	return s;
}
