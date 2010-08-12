/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Standard I/O private functions.
 */

#ifndef __STDIO_PRIV_H
#define __STDIO_PRIV_H

#include <stdbool.h>
#include <stdio.h>

#include "../libc.h"

/** Internal structure of an I/O stream (FILE). */
struct __fstream_internal {
	int fd;				/**< File descriptor the stream refers to. */
	bool err;			/**< Error indicator. */
	bool eof;			/**< End of file indicator. */
	int pushback_ch;		/**< Character pushed back with ungetc(). */
	bool have_pushback;		/**< Set to true if there is a pushed back character. */
};

/** Arguments to do_scanf. */
struct scanf_args {
	int (*getch)(void *);		/**< Get a character from the source file/string. */
	int (*putch)(int, void *);	/**< Return a character to the source file/string. */
	void *data;			/**< Data to pass to the helper functions. */
};

/** Type for a do_printf() helper function. */
typedef void (*printf_helper_t)(char, void *, int *);

extern int do_printf(printf_helper_t helper, void *data, const char *restrict fmt, va_list args) __hidden;
extern int do_scanf(struct scanf_args *data, const char *restrict fmt, va_list args) __hidden;

#endif /* __STDIO_PRIV_H */
