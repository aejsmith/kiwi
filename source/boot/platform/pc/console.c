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
 * @brief		PC console code.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <boot/console.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <platform/bios.h>

/** Convert the serial port number from the configuration to a port number. */
#if CONFIG_X86_SERIAL_PORT == 1
# define SERIAL_PORT		0x3F8
#elif CONFIG_X86_SERIAL_PORT == 2
# define SERIAL_PORT		0x2F8
#elif CONFIG_X86_SERIAL_PORT == 3
# define SERIAL_PORT		0x3E8
#elif CONFIG_X86_SERIAL_PORT == 4
# define SERIAL_PORT		0x2E8
#endif

/** VGA attributes. */
#define VGA_ATTRIB		0x0700
#define VGA_COLS		80
#define VGA_ROWS		25

/** VGA register definitions. */
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5

static void pc_console_scroll_down(void);

/** VGA memory pointer. */
static uint16_t *vga_mapping = (uint16_t *)0xB8000;

/** VGA cursor position. */
static int vga_cursor_x = 0;
static int vga_cursor_y = 0;

/** VGA scroll region. */
static int vga_scroll_y1 = 0;
static int vga_scroll_y2 = VGA_ROWS - 1;

/** Write a character to the VGA console.
 * @param ch		Character to write. */
static void pc_console_putch(char ch) {
	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(vga_cursor_x != 0) {
			vga_cursor_x--;
		} else {
			vga_cursor_x = VGA_COLS - 1;
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
		if(ch < ' ') {
			break;
		}

		vga_mapping[(vga_cursor_y * VGA_COLS) + vga_cursor_x] &= 0xFF00;
		vga_mapping[(vga_cursor_y * VGA_COLS) + vga_cursor_x] |= ch;
		vga_cursor_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(vga_cursor_x >= VGA_COLS) {
		vga_cursor_x = 0;
		if(++vga_cursor_y > vga_scroll_y2) {
			pc_console_scroll_down();
			vga_cursor_y = vga_scroll_y2;
		}
	}
}

/** Clear the VGA console. */
static void pc_console_clear(void) {
	int i, j;

	for(i = 0; i < VGA_ROWS; i++) {
		for(j = 0; j < VGA_COLS; j++) {
			vga_mapping[(i * VGA_COLS) + j] =  ' ' | VGA_ATTRIB;
		}
	}

	vga_cursor_x = vga_cursor_y = 0;
	vga_scroll_y1 = 0;
	vga_scroll_y2 = VGA_ROWS - 1;
}

/** Change the highlight on a portion of the console.
 * @param x		Start X position.
 * @param y		Start Y position.
 * @param width		Width of the highlight.
 * @param height	Height of the highlight. */
static void pc_console_highlight(int x, int y, int width, int height) {
	uint16_t word, fg, bg;
	int i, j;

	for(i = y; i < (y + height); i++) {
		for(j = x; j < (x + width); j++) {
			/* Swap the foreground/background colour. */
			word = vga_mapping[(i * VGA_COLS) + j];
			fg = (word << 4) & 0xF000;
			bg = (word >> 4) & 0x0F00;
			vga_mapping[(i * VGA_COLS) + j] = (word & 0xFF) | fg | bg;
		}
	}
}

/** Move the cursor.
 * @param x		New X position.
 * @param y		New Y position. */
static void pc_console_move_cursor(int x, int y) {
	vga_cursor_x = x;
	vga_cursor_y = y;
}

/** Set the scroll region.
 * @param y1		First row of the scroll region.
 * @param y2		Last row of the scroll region. */
static void pc_console_set_scroll_region(int y1, int y2) {
	vga_scroll_y1 = y1;
	vga_scroll_y2 = y2;
}

/** Scroll the console up by one row. */
static void pc_console_scroll_up(void) {
	int i;

	/* Shift down the content of the VGA memory.*/
	memmove(vga_mapping + ((vga_scroll_y1 + 1) * VGA_COLS),
	        vga_mapping + (vga_scroll_y1 * VGA_COLS),
	        (vga_scroll_y2 - vga_scroll_y1) * VGA_COLS * 2);

	/* Fill the first row with blanks. */
	for(i = 0; i < VGA_COLS; i++) {
		vga_mapping[(vga_scroll_y1 * VGA_COLS) + i] &= 0xFF00;
		vga_mapping[(vga_scroll_y1 * VGA_COLS) + i] |= ' ';
	}
}

/** Scroll the console down by one row. */
static void pc_console_scroll_down(void) {
	int i;

	/* Shift up the content of the VGA memory.*/
	memmove(vga_mapping + (vga_scroll_y1 * VGA_COLS),
	        vga_mapping + ((vga_scroll_y1 + 1) * VGA_COLS),
	        (vga_scroll_y2 - vga_scroll_y1) * VGA_COLS * 2);

	/* Fill the last row with blanks. */
	for(i = 0; i < VGA_COLS; i++) {
		vga_mapping[(vga_scroll_y2 * VGA_COLS) + i] &= 0xFF00;
		vga_mapping[(vga_scroll_y2 * VGA_COLS) + i] |= ' ';
	}
}

/** Read a key from the console.
 * @return		Key read from the console. */
static uint16_t pc_console_get_key(void) {
	uint8_t ascii, scan;
	bios_regs_t regs;

	bios_regs_init(&regs);
	regs.eax = 0x0000;
	bios_interrupt(0x16, &regs);

	/* Convert certain scan codes to special keys. */
	ascii = regs.eax & 0xFF;
	scan = (regs.eax >> 8) & 0xFF;
	switch(scan) {
	case 0x48:
		return CONSOLE_KEY_UP;
	case 0x50:
		return CONSOLE_KEY_DOWN;
	case 0x4B:
		return CONSOLE_KEY_LEFT;
	case 0x4D:
		return CONSOLE_KEY_RIGHT;
	default:
		return ascii;
	}
}

/** Check if a key is waiting to be read.
 * @return		Whether a key is waiting to be read. */
static bool pc_console_check_key(void) {
	bios_regs_t regs;

	bios_regs_init(&regs);
	regs.eax = 0x0001;
	bios_interrupt(0x16, &regs);
	return !(regs.eflags & X86_FLAGS_ZF);
}

/** Main console. */
static console_t pc_console = {
	.width = VGA_COLS,
	.height = VGA_ROWS,

	.putch = pc_console_putch,
	.clear = pc_console_clear,
	.highlight = pc_console_highlight,
	.move_cursor = pc_console_move_cursor,
	.set_scroll_region = pc_console_set_scroll_region,
	.scroll_up = pc_console_scroll_up,
	.scroll_down = pc_console_scroll_down,
	.get_key = pc_console_get_key,
	.check_key = pc_console_check_key,
};

#ifdef SERIAL_PORT
/** Write a character to the serial console.
 * @param ch		Character to write. */
static void serial_console_putch(char ch) {
	while(!(in8(SERIAL_PORT + 5) & 0x20));
	if(ch == '\n') {
		while(!(in8(SERIAL_PORT + 5) & 0x20));
		out8(SERIAL_PORT, '\r');
	}
	out8(SERIAL_PORT, ch);
}

/** Debug console. */
static console_t serial_console = {
	.putch = serial_console_putch,
};
#endif

/** Initialise the console. */
void console_init(void) {
#ifdef SERIAL_PORT
	uint8_t status;

	/* Only enable the serial port when it is present. */
	status = in8(SERIAL_PORT + 6);
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
		out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
		out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
		out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
		out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
		out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
		out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
		debug_console = &serial_console;
	}
#endif
	/* We don't care about the cursor, move it out the way. */
	out8(VGA_CRTC_INDEX, 14);
	out8(VGA_CRTC_DATA, (((VGA_ROWS + 1) * VGA_COLS) >> 8) & 0xFF);
	out8(VGA_CRTC_INDEX, 15);
	out8(VGA_CRTC_DATA, ((VGA_ROWS + 1) * VGA_COLS) & 0xFF);
	pc_console_clear();
	main_console = &pc_console;
}
