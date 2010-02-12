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

struct kernel_args;

extern console_t g_fb_console;
extern uint16_t g_fb_console_width;
extern uint16_t g_fb_console_height;
extern uint8_t g_fb_console_depth;

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
extern void console_init(struct kernel_args *args);
extern void console_update_boot_progress(int percent);

#endif /* __CONSOLE_H */
