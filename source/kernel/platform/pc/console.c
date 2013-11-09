/*
 * Copyright (C) 2009-2013 Alex Smith
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

#include <lib/ansi_parser.h>
#include <lib/string.h>

#include <mm/phys.h>

#include <pc/console.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>
#include <kernel.h>

/** VGA character attributes to use. */
#define VGA_ATTRIB	0x0F00

/* Support both VGA and framebuffer consoles, let KBoot choose a mode. */
KBOOT_VIDEO(KBOOT_VIDEO_LFB | KBOOT_VIDEO_VGA, 0, 0, 0);

/** VGA console lock. */
static SPINLOCK_DECLARE(vga_lock);

/** VGA console details. */
static uint16_t *vga_mapping;		/**< VGA memory mapping. */
static uint16_t vga_cols;		/**< Number of columns. */
static uint16_t vga_lines;		/**< Number of lines. */
static uint16_t vga_cursor_x;		/**< X position of the cursor. */
static uint16_t vga_cursor_y;		/**< Y position of the cursor. */

#ifdef SERIAL_PORT
/** Serial port ANSI escape parser. */
static ansi_parser_t serial_ansi_parser;
#endif

/**
 * i8042 input functions.
 */

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
static uint16_t i8042_console_poll(void) {
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
			if(code == 0xe0)
				extended = true;

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
			while((in8(0x64) & 1) == 0) {}
			in8(0x60);
		}

		extended = false;
		if(ret != 0)
			return ret;
	}
}

/** i8042 early console input operations. */
static console_in_ops_t i8042_console_in_ops = {
	.poll = i8042_console_poll,
};

/**
 * VGA console operations.
 */

/** Write a character to the VGA memory.
 * @param idx		Index to write to.
 * @param ch		Character to write. */
static inline void vga_write(size_t idx, uint16_t ch) {
	vga_mapping[idx] = ch | VGA_ATTRIB;
}

/** Write a character to the VGA console.
 * @param ch		Character to print. */
static void vga_console_putc(char ch) {
	uint16_t i;

	spinlock_lock(&vga_lock);

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(vga_cursor_x != 0) {
			vga_cursor_x--;
		} else {
			vga_cursor_x = vga_cols - 1;
			vga_cursor_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		vga_cursor_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		vga_cursor_x = 0;
		vga_cursor_y++;
		break;
	case '\t':
		vga_cursor_x += 8 - (vga_cursor_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ')
			break;

		vga_write((vga_cursor_y * vga_cols) + vga_cursor_x, ch);
		vga_cursor_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(vga_cursor_x >= vga_cols) {
		vga_cursor_x = 0;
		vga_cursor_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(vga_cursor_y >= vga_lines) {
		memmove(vga_mapping, vga_mapping + vga_cols, (vga_lines - 1) * vga_cols * 2);

		for(i = 0; i < vga_cols; i++)
			vga_write(((vga_lines - 1) * vga_cols) + i, ' ');

		vga_cursor_y = vga_lines - 1;
	}

	/* Move the hardware cursor to the new position. */
	out8(VGA_CRTC_INDEX, 14);
	out8(VGA_CRTC_DATA, ((vga_cursor_y * vga_cols) + vga_cursor_x) >> 8);
	out8(VGA_CRTC_INDEX, 15);
	out8(VGA_CRTC_DATA, (vga_cursor_y * vga_cols) + vga_cursor_x);

	spinlock_unlock(&vga_lock);
}

/** Early initialization of the VGA console.
 * @param video		KBoot video tag. */
static void vga_console_early_init(kboot_tag_video_t *video) {
	vga_mapping = (uint16_t *)((ptr_t)video->vga.mem_virt);
	vga_cols = video->vga.cols;
	vga_lines = video->vga.lines;
	vga_cursor_x = video->vga.x;
	vga_cursor_y = video->vga.y;

	vga_console_putc('\n');
}

/** Late initialization of the VGA console.
 * @param video		KBoot video tag. */
static void vga_console_init(kboot_tag_video_t *video) {
	/* Create our own mapping of VGA memory to replace KBoot's mapping. */
	vga_mapping = phys_map(video->vga.mem_phys, vga_cols * vga_lines * 2, MM_BOOT);
}

/** VGA console output operations. */
static console_out_ops_t vga_console_out_ops = {
	.init = vga_console_init,
	.putc = vga_console_putc,
};

/**
 * Serial console operations.
 */

#if SERIAL_PORT

/** Write a character to the serial console.
 * @param ch		Character to print. */
static void serial_console_putc(char ch) {
	if(ch == '\n')
		serial_console_putc('\r');

	out8(SERIAL_PORT, ch);
	while(!(in8(SERIAL_PORT + 5) & (1<<5)));
}

/** Read a character from the serial console.
 * @return		Character read, or 0 if none available. */
static uint16_t serial_console_poll(void) {
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
			if(converted)
				return converted;
		}
	}

	return 0;
}

/** Early initialization of the serial console.
 * @return		Whether the serial console is present. */
static bool serial_console_early_init(void) {
	uint8_t status;

	/* Check whether the port is present. */
	status = in8(SERIAL_PORT + 6);
	if(!(status & ((1<<4) | (1<<5))) || status == 0xFF)
		return false;

	out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
	out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
	out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
	out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
	out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
	out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
	out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */

	/* Wait for transmit to be empty. */
	while(!(in8(SERIAL_PORT + 5) & (1<<5)))
		;

	ansi_parser_init(&serial_ansi_parser);
	return true;
}

/** Serial port console output operations. */
static console_out_ops_t serial_console_out_ops = {
	.putc = serial_console_putc,
};

/** Serial console input operations. */
static console_in_ops_t serial_console_in_ops = {
	.poll = serial_console_poll,
};

#endif

/*
 * Initialization functions.
 */

/** Set up the debug console.
 * @param video		KBoot video tag. */
__init_text void platform_console_early_init(kboot_tag_video_t *video) {
	#ifdef SERIAL_PORT
	/* Register the serial console for debug output. */
	if(serial_console_early_init()) {
		debug_console.out = &serial_console_out_ops;
		debug_console.in = &serial_console_in_ops;
	}
	#endif

	/* If we have a VGA console, enable it. */
	if(video && video->type == KBOOT_VIDEO_VGA) {
		vga_console_early_init(video);
		main_console.out = &vga_console_out_ops;
	}

	/* Register the early keyboard input operations. */
	main_console.in = &i8042_console_in_ops;
}	
