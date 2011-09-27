/*
 * Copyright (C) 2009-2011 Alex Smith
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

/** Kernel console implementation structure. */
typedef struct console {
	list_t header;			/**< Link to the console list. */
	int min_level;			/**< Minimum log level that the console will receive. */

	/** Write a character to the console.
	 * @param ch		Character to write. */
	void (*putc)(unsigned char ch);

	/** Get a character from the console.
	 * @return		Character read, or 0 if no character available. */
	uint16_t (*getc)(void);
} console_t;

/** Special console key definitions. */
#define CONSOLE_KEY_UP		0x100
#define CONSOLE_KEY_DOWN	0x101
#define CONSOLE_KEY_LEFT	0x102
#define CONSOLE_KEY_RIGHT	0x103
#define CONSOLE_KEY_HOME	0x104
#define CONSOLE_KEY_END		0x105
#define CONSOLE_KEY_PGUP	0x106
#define CONSOLE_KEY_PGDN	0x107
#define CONSOLE_KEY_DELETE	0x108

extern void console_putc_unsafe(unsigned char ch);
extern uint16_t console_getc_unsafe(void);

extern void console_register(console_t *console);
extern void console_unregister(console_t *console);

extern void platform_console_early_init(void);
extern void platform_console_init(void);

extern void console_early_init(void);
extern void console_init(void);
extern void console_update_boot_progress(int percent);

/** Buffer length for ANSI escape code parser. */
#define ANSI_PARSER_BUFFER_LEN	3

/** ANSI escape code parser structure. */
typedef struct ansi_parser {
	/** Buffer containing collected sequence. */
	char buffer[ANSI_PARSER_BUFFER_LEN];

	int length;			/**< Buffer length. */
} ansi_parser_t;

extern uint16_t ansi_parser_filter(ansi_parser_t *parser, unsigned char ch);
extern void ansi_parser_init(ansi_parser_t *parser);

/** Framebuffer information structure. */
typedef struct fb_info {
	uint16_t width;			/**< Width of the framebuffer. */
	uint16_t height;		/**< Height of the framebuffer. */
	uint16_t depth;			/**< Colour depth of the framebuffer. */
	phys_ptr_t addr;		/**< Physical address of the framebuffer. */
} fb_info_t;

/** Framebuffer console control operations. */
#define FB_CONSOLE_INFO		1	/**< Get information. */
#define FB_CONSOLE_CONFIGURE	2	/**< Move to a new framebuffer. */
#define FB_CONSOLE_ACQUIRE	3	/**< Take control of the framebuffer (prevent kernel output). */
#define FB_CONSOLE_RELEASE	4	/**< Release control of the framebuffer. */

extern void fb_console_control(unsigned op, fb_info_t *info);

#endif /* __CONSOLE_H */
