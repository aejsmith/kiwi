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
