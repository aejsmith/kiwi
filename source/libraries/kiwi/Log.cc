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
 * @brief		Internal libkiwi logging functions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "Internal.h"

/** Print out a message.
 * @param stream	Stream to print to.
 * @param prefix	What to print before the actual message.
 * @param fmt		Format string.
 * @param args		Argument list.
 * @param terminate	Whether to terminate the program after printing the
 *			message. */
static void do_log_message(FILE *stream, const char *prefix, const char *fmt,
                           va_list args, bool terminate) {
	fprintf(stream, "*** libkiwi-%s: ", prefix);
	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");
	if(terminate) {
		abort();
	}
}

#if CONFIG_DEBUG
/** Print a debug message. */
void libkiwi_debug(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	do_log_message(stdout, "DEBUG", fmt, args, false);
	va_end(args);
}
#endif

/** Print a warning message. */
void libkiwi_warn(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	do_log_message(stderr, "WARN", fmt, args, false);
	va_end(args);
}

/** Print a fatal error message and exit.
 * @todo		Fatal error messages should pop up a message box on the
 *			GUI so programs don't just drop dead without any
 *			indication why. */
void libkiwi_fatal(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	do_log_message(stderr, "FATAL", fmt, args, true);
	va_end(args);
}