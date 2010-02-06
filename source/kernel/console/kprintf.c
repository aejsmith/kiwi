/*
 * Copyright (C) 2008-2009 Alex Smith
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

#include <console/kprintf.h>

#include <lib/do_printf.h>

#include <sync/spinlock.h>

static SPINLOCK_DECLARE(kprintf_lock);

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Pointer to output level integer.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	int level = *(int *)data;

	console_putch(level, ch);
	*total = *total + 1;
}

/** Output a formatted message.
 * 
 * Outputs a formatted message to the console. The level parameter is passed
 * onto console_putch(), and should be one of the log levels defined in
 * console.h.
 *
 * @param level		Kernel log level.
 * @param format	Format string used to create the message.
 * @param args		Arguments to substitute into format.
 */
int kvprintf(int level, const char *format, va_list args) {
	int ret;

	if(level != LOG_NONE) {
		spinlock_lock(&kprintf_lock, 0);
		ret = do_printf(kvprintf_helper, &level, format, args);
		spinlock_unlock(&kprintf_lock);
	} else {
		ret = do_printf(kvprintf_helper, &level, format, args);
	}

	return ret;
}

/** Output a formatted message to the kernel console.
 * 
 * Outputs a formatted string to the kernel console. Where the message is
 * displayed and whether it is displayed depends on the level specified.
 *
 * @param level		Log level.
 * @param format	Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 */
int kprintf(int level, const char *format, ...) {
	int ret;
	va_list args;

	va_start(args, format);
	ret = kvprintf(level, format, args);
	va_end(args);

	return ret;
}
