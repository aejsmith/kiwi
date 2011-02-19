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
 * @brief		String unformatting function.
 */

#include <stdarg.h>
#include <stdio.h>

#include "stdio_priv.h"

struct vsscanf_data {
	unsigned char *buf;
};

/** Helper function for vsscanf (get character). */
static int vsscanf_getch(void *_data) {
	struct vsscanf_data *data = (struct vsscanf_data *)_data;
	unsigned int ret;

	ret = *(data->buf++);
	return (ret) ? (int)ret : -1;
}

/** Helper function for vsscanf (put character). */
static int vsscanf_putch(int c, void *_data) {
	struct vsscanf_data *data = (struct vsscanf_data *)_data;

	return (*(--data->buf) == c)? c : -1;
}

/** Unformat a buffer.
 *
 * Unformats a buffer into a list of arguments according to the given format
 * string.
 *
 * @param buf		Buffer to unformat.
 * @param fmt		Format string.
 * @param args		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int vsscanf(const char *restrict buf, const char *restrict fmt, va_list args) {
	struct vsscanf_data data = { (unsigned char *)buf };
	struct scanf_args sdata = { vsscanf_getch, vsscanf_putch, &data };

	return do_scanf(&sdata, fmt, args);
}

/** Unformat a buffer.
 *
 * Unformats a buffer into a list of arguments according to the given format
 * string.
 *
 * @param buf		Buffer to unformat.
 * @param fmt		Format string.
 * @param ...		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int sscanf(const char *restrict buf, const char *restrict fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsscanf(buf, fmt, args);
	va_end(args);

	return ret;
}
