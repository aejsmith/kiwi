/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		String unformatting function.
 */

#include <stdarg.h>
#include <stdio.h>

#include "stdio_priv.h"

/** Unformat data from a stream.
 *
 * Unformats data from a file stream into a list of arguments according to the
 * given format string.
 *
 * @param stream	Stream to read from.
 * @param fmt		Format string.
 * @param args		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int vfscanf(FILE *restrict stream, const char *restrict fmt, va_list args) {
	struct scanf_args sdata = { (int (*)(void *))fgetc, (int (*)(int, void *))ungetc, stream };

	return do_scanf(&sdata, fmt, args);
}

/** Unformat data from a stream.
 *
 * Unformats data from a file stream into a list of arguments according to the
 * given format string.
 *
 * @param stream	Stream to read from.
 * @param fmt		Format string.
 * @param ...		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int fscanf(FILE *restrict stream, const char *restrict fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfscanf(stream, fmt, args);
	va_end(args);

	return ret;
}

/** Unformat data from standard input.
 *
 * Unformats data from standard input into a list of arguments according to
 * the given format string.
 *
 * @param fmt		Format string.
 * @param args		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int vscanf(const char *restrict fmt, va_list args) {
	return vfscanf(stdin, fmt, args);
}

/** Unformat data from standard input.
 *
 * Unformats data from standard input into a list of arguments according to
 * the given format string.
 *
 * @param fmt		Format string.
 * @param ...		Pointers to values to set to unformatted arguments.
 *
 * @return		Number of input items matched.
 */
int scanf(const char *restrict fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfscanf(stdin, fmt, args);
	va_end(args);

	return ret;
}
