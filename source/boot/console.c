/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Console functions.
 */

#include <boot/console.h>
#include <lib/printf.h>

/** Debug log size. */
#define DEBUG_LOG_SIZE		8192

/** Debug output log. */
char debug_log[DEBUG_LOG_SIZE];
static size_t debug_log_offset = 0;

/** Main console. */
console_t *main_console = NULL;

/** Debug console. */
console_t *debug_console = NULL;

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Console to use.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	if(main_console) {
		main_console->putch(ch);
	}
	*total = *total + 1;
}

/** Output a formatted message to the console.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 * @return		Number of characters printed. */
int kvprintf(const char *fmt, va_list args) {
	return do_printf(kvprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the console.
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 * @return		Number of characters printed. */
int kprintf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = kvprintf(fmt, args);
	va_end(args);

	return ret;
}

/** Helper for dvprintf().
 * @param ch		Character to display.
 * @param data		Console to use.
 * @param total		Pointer to total character count. */
static void dvprintf_helper(char ch, void *data, int *total) {
	if(debug_console) {
		debug_console->putch(ch);
	}
	if(debug_log_offset < (DEBUG_LOG_SIZE - 1)) {
		debug_log[debug_log_offset++] = ch;
		debug_log[debug_log_offset] = 0;
	}
	*total = *total + 1;
}

/** Output a formatted message to the debug console.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 * @return		Number of characters printed. */
int dvprintf(const char *fmt, va_list args) {
	return do_printf(dvprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the debug console.
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 * @return		Number of characters printed. */
int dprintf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = dvprintf(fmt, args);
	va_end(args);

	return ret;
}
