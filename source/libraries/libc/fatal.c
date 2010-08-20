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
 * @brief		C library fatal error functions.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "libc.h"
#include "stdio/stdio_priv.h"

/** Helper for libc_fatal().
 * @param ch		Character to print.
 * @param data		Pointer to file stream.
 * @param total		Pointer to total character count. */
static void libc_fatal_helper(char ch, void *data, int *total) {
	if(data) {
		fputc(ch, (FILE *)data);
	}
		
	*total = *total + 1;
}

/** Print out a fatal error and terminate the process.
 * @param fmt		Format string.
 * @param ...		Arguments to substitute into format. */
void libc_fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_printf(libc_fatal_helper, stderr, "*** libc fatal: ", args);
	va_end(args);

	va_start(args, fmt);
	do_printf(libc_fatal_helper, stderr, fmt, args);
	va_end(args);

	if(stderr) {
		fputc('\n', stderr);
	}

	abort();
}

/** Handle a call to a stub function.
 * @param name		Name of function.
 * @param fatal		Whether the error is considered fatal. */
void libc_stub(const char *name, bool fatal) {
	if(fatal) {
		libc_fatal("unimplemented function: %s", name);
	} else {
		fprintf(stderr, "STUB: %s\n", name);
		errno = ENOSYS;
	}
}

/** Print out an assertion fail message.
 * @param cond		Condition.
 * @param file		File it occurred in.
 * @param line		Line number.
 * @param func		Function name. */
void __assert_fail(const char *cond, const char *file, unsigned int line, const char *func) {
	if(!func) {
		fprintf(stderr, "Assertion '%s' failed at %s:%d\n", cond, file, line);
	} else {
		fprintf(stderr, "Assertion '%s' failed at %s:%d (%s)\n", cond, file, line, func);
	}
	abort();
}
