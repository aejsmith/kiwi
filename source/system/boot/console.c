/*
 * Copyright (C) 2010 Alex Smith
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
