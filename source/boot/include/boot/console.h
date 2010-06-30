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
 * @brief		Console functions.
 */

#ifndef __BOOT_CONSOLE_H
#define __BOOT_CONSOLE_H

#include <stdarg.h>
#include <types.h>

/** Structure describing a console. */
typedef struct console {
	int width;		/**< Width of the console (columns). */
	int height;		/**< Height of the console (rows). */

	/** Write a character to the console.
	 * @param ch		Character to write. */
	void (*putch)(char ch);

	/** Clear the console.
	 * @note		Also resets the scroll region. */
	void (*clear)(void);

	/** Change the highlight on a portion of the console.
	 * @note		This reverses whatever the current state of
	 *			each character is, e.g. if something is already
	 *			highlighted, it will become unhighlighted.
	 * @param x		Start X position.
	 * @param y		Start Y position.
	 * @param width		Width of the highlight.
	 * @param height	Height of the highlight. */
	void (*highlight)(int x, int y, int width, int height);

	/** Move the cursor.
	 * @param x		New X position.
	 * @param y		New Y position. */
	void (*move_cursor)(int x, int y);

	/** Set the scroll region.
	 * @param y1		First row in the scroll region.
	 * @param y2		Last row in the scroll region. */
	void (*set_scroll_region)(int y1, int y2);

	/** Scroll the scroll region up (move contents down). */
	void (*scroll_up)(void);

	/** Scroll the scroll region down (move contents up). */
	void (*scroll_down)(void);

	/** Read a keypress from the console.
	 * @return		Key read from the console. */
	uint16_t (*get_key)(void);

	/** Check if input is available.
	 * @return		Whether input is available. */
	bool (*check_key)(void);
} console_t;

/** Special key codes. */
#define CONSOLE_KEY_UP		0x100
#define CONSOLE_KEY_DOWN	0x101
#define CONSOLE_KEY_LEFT	0x102
#define CONSOLE_KEY_RIGHT	0x103

extern char debug_log[];
extern size_t debug_log_offset;

extern console_t *main_console;
extern console_t *debug_console;

extern int kvprintf(const char *fmt, va_list args);
extern int kprintf(const char *fmt, ...) __printf(1, 2);
extern int dvprintf(const char *fmt, va_list args);
extern int dprintf(const char *fmt, ...) __printf(1, 2);

extern void console_init(void);

#endif /* __BOOT_CONSOLE_H */
