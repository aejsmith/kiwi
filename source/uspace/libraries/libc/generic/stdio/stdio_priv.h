/* Kiwi C library - Standard I/O private functions
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
 * @brief		Standard I/O private functions.
 */

#ifndef __STDIO_PRIV_H
#define __STDIO_PRIV_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** Internal structure of an I/O stream (FILE). */
#if 0
struct __libc_fstream {
        int fd;				/**< File descriptor backing the stream. */
        int err;			/**< Error indicator. */
        int eof;			/**< End of file indicator. */
	int buf_mode;			/**< Buffering mode. */
	char *buf;			/**< Input/output buffer. */
	size_t buf_size;		/**< Size of the buffer. */
	int pushback_ch;		/**< Character pushed back with ungetc(). */
	bool have_pushback;		/**< Set to true if there is a pushed back character. */
};
#endif

#if 0
/** Arguments to do_scanf. */
struct scanf_args {
	int (*getch)(void *);		/**< Get a character from the source file/string. */
	int (*putch)(int, void *);	/**< Return a character to the source file/string. */
	void *data;			/**< Data to pass to the helper functions. */
};
#endif

/** Type for a do_printf() helper function. */
typedef void (*printf_helper_t)(char, void *, int *);

extern int do_printf(printf_helper_t helper, void *data, const char *fmt, va_list args);
//extern int do_scanf(struct scanf_args *data, const char *fmt, va_list args);

#endif /* __STDIO_PRIV_H */
