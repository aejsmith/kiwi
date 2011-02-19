/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Formatted output functions.
 */

#include "stdio_priv.h"

/** Helper for vfprintf().
 * @param ch		Character to print.
 * @param data		Pointer to file stream.
 * @param total		Pointer to total character count. */
static void vfprintf_helper(char ch, void *data, int *total) {
	fputc(ch, (FILE *)data);
	*total = *total + 1;
}

/** Output a formatted message to a file stream.
 * 
 * Outputs a formatted string to a file stream.
 *
 * @param stream	Stream to output to.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 *
 * @return		Number of characters printed.
 */
int vfprintf(FILE *restrict stream, const char *restrict fmt, va_list args) {
	return do_printf(vfprintf_helper, stream, fmt, args);
}

/** Output a formatted message to a file stream.
 * 
 * Outputs a formatted string to a file stream.
 *
 * @param stream	Stream to output to.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 *
 * @return		Number of characters printed.
 */
int fprintf(FILE *restrict stream, const char *restrict fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stream, fmt, args);
	va_end(args);

	return ret;
}

/** Output a formatted message.
 * 
 * Outputs a formatted string to the console.
 *
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 *
 * @return		Number of characters printed.
 */
int vprintf(const char *restrict fmt, va_list args) {
	return vfprintf(stdout, fmt, args);
}

/** Output a formatted message.
 * 
 * Outputs a formatted string to the console.
 *
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 *
 * @return		Number of characters printed.
 */
int printf(const char *restrict fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}
