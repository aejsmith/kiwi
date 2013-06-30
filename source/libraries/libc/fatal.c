/*
 * Copyright (C) 2009-2010 Alex Smith
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
