/* Fatal error function
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
 * @brief		Fatal error function.
 */

#include "libsystem.h"
#include "stdio/stdio_priv.h"

/** Helper for __libsystem_fatal().
 * @param ch		Character to print.
 * @param data		Pointer to file stream.
 * @param total		Pointer to total character count. */
static void __libsystem_fatal_helper(char ch, void *data, int *total) {
	if(data) {
		fputc(ch, (FILE *)data);
	} else {
		kputch(ch);
	}
		
	*total = *total + 1;
}

/** Print out a fatal error and terminate the process.
 * @param fmt		Format string.
 * @param ...		Arguments to substitute into format. */
void __libsystem_fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_printf(__libsystem_fatal_helper, stderr, "*** libsystem fatal: ", args);
	va_end(args);

	va_start(args, fmt);
	do_printf(__libsystem_fatal_helper, stderr, fmt, args);
	va_end(args);

	if(stderr) {
		fputc('\n', stderr);
	} else {
		kputch('\n');
	}

	process_exit(1);
}
