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

/** Helper for kprintf()/dprintf().
 * @param ch		Character to display.
 * @param data		Console to use.
 * @param total		Pointer to total character count. */
static void printf_helper(char ch, void *data, int *total) {
	console_t *console = (console_t *)data;
	console->putch(ch);
	*total = *total + 1;
}

/** Output a formatted message to the console.
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 * @return		Number of characters printed. */
int kprintf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = do_printf(printf_helper, &main_console, fmt, args);
	va_end(args);

	return ret;
}

/** Output a formatted message to the debug console.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 * @return		Number of characters printed. */
int dvprintf(const char *fmt, va_list args) {
	return do_printf(printf_helper, &debug_console, fmt, args);
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
