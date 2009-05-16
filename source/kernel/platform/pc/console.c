/* Kiwi x86 console code
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
 * @brief		x86 console code.
 */

#include <arch/io.h>
#include <arch/mem.h>

#include <console/console.h>

#include <mm/page.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <fatal.h>

extern void console_late_init(void);

/*
 * VGA console functions.
 */

/** Set attributes of a character (white on black). */
#define ATTRIB(ch)	((uint16_t)ch | ((uint16_t)0x0F << 8))

/** VGA console size definitions. */
#define VGA_COLS	80	/**< Number of columns on the VGA console. */
#define VGA_ROWS	25	/**< Number of rows on the VGA console. */

/** Pointer to VGA mapping. */
static uint16_t *vga_mapping = (uint16_t *)0xB8000;

/** VGA cursor position variables. */
static int vga_x = 0;		/**< X position of the cursor. */
static int vga_y = 0;		/**< Y position of the cursor. */

/** Scroll the VGA console.
 *
 * Moves the contents of the VGA console up by one line.
 */
static void vga_console_scroll(void) {
	int i;

	memmove(vga_mapping, vga_mapping + VGA_COLS, (VGA_ROWS - 1) * VGA_COLS * 2);

	for(i = 0; i < VGA_COLS; i++) {
		vga_mapping[((VGA_ROWS - 1) * VGA_COLS) + i] = ATTRIB(' ');
	}

	vga_y = VGA_ROWS - 1;
}

/** Move the VGA cursor.
 *
 * Moves the VGA cursor to a new position.
 */
static void vga_console_move_csr(void) {
	out8(0x3D4, 14);
	out8(0x3D5, ((vga_y * VGA_COLS) + vga_x) >> 8);
	out8(0x3D4, 15);
	out8(0x3D5, (vga_y * VGA_COLS) + vga_x);
}

/** Clears the VGA console.
 *
 * Clears the contents of the VGA console.
 */
static void vga_console_clear(void) {
	int i, j;

	for(i = 0; i < VGA_ROWS; i++) {
		for(j = 0; j < VGA_COLS; j++) {
			vga_mapping[(i * VGA_COLS) + j] = ATTRIB(' ');
		}
	}

	/* Update X/Y psoition and move the hardware cursor. */
	vga_x = 0;
	vga_y = 0;
	vga_console_move_csr();
}

/** Write a character to the VGA console.
 *
 * Writes a character to the VGA console at the current X/Y position.
 *
 * @param ch		Character to print.
 */
static void vga_console_putch(unsigned char ch) {
	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(vga_x != 0) {
			vga_x--;
		} else {
			vga_x = VGA_COLS - 1;
			vga_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		vga_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		vga_x = 0;
		vga_y++;
		break;
	case '\t':
		vga_x = (vga_x + 8) % 8;
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		vga_mapping[(vga_y * VGA_COLS) + vga_x] = ATTRIB(ch);
		vga_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(vga_x >= VGA_COLS) {
		vga_x = 0;
		vga_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(vga_y >= VGA_ROWS) {
		vga_console_scroll();
	}

	/* Move the hardware cursor to the new position. */
	vga_console_move_csr();
}

static console_t vga_console = {
	.debug = false,
	.init = vga_console_clear,
	.putch = vga_console_putch,
};

/*
 * Serial console functions.
 */

#if CONFIG_DEBUG
/** Initialize the serial console.
 *
 * Initialization function for the serial port console.
 */
static void serial_console_init(void) {
        out8(CONFIG_X86_DEBUG_PORT + 1, 0x00);  /* Disable all interrupts */
        out8(CONFIG_X86_DEBUG_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
        out8(CONFIG_X86_DEBUG_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
        out8(CONFIG_X86_DEBUG_PORT + 1, 0x00);  /*                  (hi byte) */
        out8(CONFIG_X86_DEBUG_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
        out8(CONFIG_X86_DEBUG_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
        out8(CONFIG_X86_DEBUG_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

/** Print a character to the serial console.
 *
 * Prints a character on the serial port console.
 *
 * @param ch		Character to print.
 */
static void serial_console_putch(unsigned char ch) {
	while(!(in8(CONFIG_X86_DEBUG_PORT + 5) & 0x20));
	if(ch == '\n') {
		serial_console_putch('\r');
	}
	out8(CONFIG_X86_DEBUG_PORT, ch);
}

/** Serial port console. */
static console_t serial_console = {
	.debug = true,
	.init = serial_console_init,
	.putch = serial_console_putch,
};
#endif

/*
 * Main functions.
 */

/** Set up the console. */
void console_early_init(void) {
#if CONFIG_DEBUG
	uint8_t status = in8(CONFIG_X86_DEBUG_PORT + 6);

	/* Only add the serial device when it is present. */
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		console_register(&serial_console);
	}
#endif
	console_register(&vga_console);
}

/** Relocate the VGA memory mapping. */
void console_late_init(void) {
	void *addr;

	addr = page_phys_map(0xB8000, VGA_COLS * VGA_ROWS * 2, 0);
	if(addr == NULL) {
		fatal("Could not map VGA memory");
	}

	vga_mapping = (uint16_t *)addr;
}
