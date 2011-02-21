/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel console functions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <lib/list.h>
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

extern console_t fb_console;
extern uint16_t fb_console_width;
extern uint16_t fb_console_height;
extern uint8_t fb_console_depth;

extern int kvprintf(int level, const char *fmt, va_list args);
extern int kprintf(int level, const char *fmt, ...) __printf(2, 3);

extern void console_putch(int level, char ch);
extern void console_register(console_t *cons);
extern void console_unregister(console_t *cons);

extern int kdbg_cmd_log(int argc, char **argv);

extern void fb_console_reconfigure(uint16_t width, uint16_t height, uint8_t depth,
                                   phys_ptr_t addr);
extern void fb_console_reset(void);

extern void console_early_init(void);
extern void console_init(void);
extern void console_update_boot_progress(int percent);

#endif /* __CONSOLE_H */
