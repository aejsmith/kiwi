/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Internal libkiwi logging functions.
 */

#include <kernel/process.h>
#include <kernel/thread.h>

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
	fprintf(stream, "*** %s (%d:%d): ", prefix, kern_process_id(-1), kern_thread_id(-1));
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
	do_log_message(stderr, "WARNING", fmt, args, false);
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
