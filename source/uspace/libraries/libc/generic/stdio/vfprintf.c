/* Kiwi C library - Formatted output functions
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
 * @brief		Formatted output functions.
 */

#include "stdio_priv.h"

#if 0
/** Helper for vfprintf().
 * @param ch		Character to print.
 * @param data		Pointer to file stream.
 * @param total		Pointer to total character count. */
static void vfprintf_helper(char ch, void *data, int *total) {
	fputc(ch, (FILE *)data->data);
	data->total++;
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
int vfprintf(FILE *stream, const char *fmt, va_list args) {
	struct printf_args data = { vfprintf_helper, stream, 0 };
	return do_printf(&data, fmt, args);
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
int fprintf(FILE *stream, const char *fmt, ...) {
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vfprintf(stream, fmt, args);
	va_end(args);

	return ret;
}
#endif

extern void putch(char ch);

/** Helper for vprintf().
 * @param ch		Character to print.
 * @param data		Pointer to file stream.
 * @param total		Pointer to total character count. */
static void vprintf_helper(char ch, void *data, int *total) {
	putch(ch);
	*total = *total + 1;
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
int vprintf(const char *fmt, va_list args) {
	return do_printf(vprintf_helper, NULL, fmt, args);
//	return vfprintf(stdout, fmt, args);
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
int printf(const char *fmt, ...) {
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vprintf(fmt, args);
	va_end(args);

	return ret;
}
