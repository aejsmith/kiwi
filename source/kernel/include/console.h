/*
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
 * @brief		Kernel console functions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <types/list.h>
#include <stdarg.h>

/** Kernel console implementation structure. */
typedef struct console {
	list_t header;			/**< Link to console list. */
	int min_level;			/**< Console will not receive output below this level. */
	bool inhibited;			/**< Whether output is inhibited. */

	/** Initialise the console. */
	void (*init)(void);

	/** Write a character to the console. */
	void (*putch)(unsigned char ch);
} console_t;

/** Console log levels. */
#define LOG_DEBUG	1		/**< Debug message. */
#define LOG_NORMAL	2		/**< Normal message. */
#define LOG_WARN	3		/**< Warning message. */
#define LOG_NONE	4		/**< Do not log the message (for fatal/KDBG). */

/** Font definitions. */
#define FONT_WIDTH		8
#define FONT_HEIGHT		8
#define FONT_DATA		console_font_8x8

extern unsigned char console_font_8x8[2048];

extern int kvprintf(int level, const char *fmt, va_list args);
extern int kprintf(int level, const char *fmt, ...) __printf(2, 3);

extern void console_putch(int level, char ch);
extern void console_register(console_t *cons);
extern void console_unregister(console_t *cons);

extern int kdbg_cmd_log(int argc, char **argv);

extern void console_early_init(void);

#endif /* __CONSOLE_H */
