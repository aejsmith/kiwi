/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		PC console code.
 */

#include <arch/io.h>

#include <pc/console.h>

#include <console.h>
#include <kernel.h>

/** Keyboard code definitions */
#define LEFT_CTRL	0x1D
#define RIGHT_CTRL	0x1D
#define LEFT_ALT	0x38
#define RIGHT_ALT	0x38
#define LEFT_SHIFT	0x2A
#define RIGHT_SHIFT	0x36

#ifdef SERIAL_PORT
/** Serial port ANSI escape parser. */
static ansi_parser_t serial_ansi_parser;
#endif

#if CONFIG_PC_KBD_LAYOUT_UK
/** Lower case keyboard layout - United Kingdom. */
static const unsigned char kbd_layout[128] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, 0, 0,
	'#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\'
};

/** Shift keyboard layout - United Kingdom. */
static const unsigned char kbd_layout_shift[128] = {
	0, 0, '!', '"', 156, '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', 0, 0,
	'~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '|'
};
#elif CONFIG_PC_KBD_LAYOUT_US
/** Lower case keyboard layout - United States. */
static const unsigned char kbd_layout[128] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+'
};

/** Shift keyboard layout - United States. */
static const unsigned char kbd_layout_shift[128] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
	'|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+'
};
#endif

/** Extended keyboard layout. */
static const uint16_t kbd_layout_extended[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	CONSOLE_KEY_HOME, CONSOLE_KEY_UP, CONSOLE_KEY_PGUP, 0,
	CONSOLE_KEY_LEFT, 0, CONSOLE_KEY_RIGHT, 0, CONSOLE_KEY_END,
	CONSOLE_KEY_DOWN, CONSOLE_KEY_PGDN, 0, CONSOLE_KEY_DELETE
};

/** Read a character from the i8042 keyboard.
 * @return		Character read, or 0 if none available. */
static uint16_t i8042_console_getc(void) {
	static bool shift = false;
	static bool ctrl = false;
	static bool alt = false;
	static bool extended = false;

	unsigned char code;
	uint16_t ret;

	while(true) {
		/* Check for keyboard data. */
		code = in8(0x64);
		if(code & (1<<0)) {
			if(code & (1<<5)) {
				/* Mouse data, discard. */
				in8(0x60);
				continue;
			}
		} else {
			return 0;
		}

		/* Read the code. */
		code = in8(0x60);

		/* Check for an extended code. */
		if(code >= 0xe0) {
			if(code == 0xe0) {
				extended = true;
			}
			continue;
		}

		/* Handle key releases. */
		if(code & 0x80) {
			code &= 0x7F;

			if(code == LEFT_SHIFT || code == RIGHT_SHIFT) {
				shift = false;
			} else if(code == LEFT_CTRL || code == RIGHT_CTRL) {
				ctrl = false;
			} else if(code == LEFT_ALT || code == RIGHT_ALT) {
				alt = false;
			}
			extended = false;
			continue;
		}

		if(!extended) {
			if(code == LEFT_SHIFT || code == RIGHT_SHIFT) {
				shift = true;
				continue;
			} else if(code == LEFT_CTRL || code == RIGHT_CTRL) {
				ctrl = true;
				continue;
			} else if(code == LEFT_ALT || code == RIGHT_ALT) {
				alt = true;
				continue;
			}
		}

		/* Work out the return code. */
		ret = (extended)
			? kbd_layout_extended[code]
			: ((shift) ? kbd_layout_shift[code] : kbd_layout[code]);

		/* Little hack so that pressing Enter won't result in an extra
		 * newline being sent. */
		if(ret == '\n') {
			while((in8(0x64) & 1) == 0) {};
			in8(0x60);
		}

		extended = false;
		if(ret != 0) {
			return ret;
		}
	}
}

/** PC console. */
static console_in_ops_t i8042_console_in_ops = {
	.getc = i8042_console_getc,
};

#if SERIAL_PORT
/** Write a character to the serial console.
 * @param ch		Character to print. */
static void serial_console_putc(char ch) {
	if(ch == '\n') {
		serial_console_putc('\r');
	}

	out8(SERIAL_PORT, ch);
	while(!(in8(SERIAL_PORT + 5) & 0x20));
}

/** Serial port console output operations. */
static console_out_ops_t serial_console_out_ops = {
	.putc = serial_console_putc,
};

/** Read a character from the serial console.
 * @return		Character read, or 0 if none available. */
static uint16_t serial_console_getc(void) {
	unsigned char ch = in8(SERIAL_PORT + 6);
	uint16_t converted;

	if((ch & ((1<<4) | (1<<5))) && ch != 0xFF) {
		if(in8(SERIAL_PORT + 5) & 0x01) {
			ch = in8(SERIAL_PORT);

			/* Convert CR to NL, and DEL to Backspace. */
			if(ch == '\r') {
				ch = '\n';
			} else if(ch == 0x7f) {
				ch = '\b';
			}

			/* Handle escape sequences. */
			converted = ansi_parser_filter(&serial_ansi_parser, ch);
			if(converted) {
				return converted;
			}
		}
	}

	return 0;
}

/** Serial port console input operations. */
static console_in_ops_t serial_console_in_ops = {
	.getc = serial_console_getc,
};
#endif

/** Set up the debug console. */
__init_text void platform_console_early_init(void) {
#ifdef SERIAL_PORT
	uint8_t status = in8(SERIAL_PORT + 6);

	/* Only add the serial device when it is present. */
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
		out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
		out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
		out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
		out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
		out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
		out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */

		/* Wait for transmit to be empty. */
		while(!(in8(SERIAL_PORT + 5) & 0x20));

		ansi_parser_init(&serial_ansi_parser);
		debug_console_ops = &serial_console_out_ops;
		console_register_in_ops(&serial_console_in_ops);
	}
#endif
	/* Register the i8042 input device. */
	console_register_in_ops(&i8042_console_in_ops);
}

/** Set up the console. */
__init_text void platform_console_init(void) {
	/* Use the framebuffer set up by KBoot. */
	fb_console_init();
}
