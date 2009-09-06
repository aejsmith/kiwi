/* Kiwi PC console code
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

#include <arch/io.h>
#include <arch/memmap.h>

#include <mm/page.h>

#include <platform/console.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <fatal.h>

#if 0
# pragma mark VGA console functions.
#endif

/** Puts a character into the VGA memory. */
#define VGA_WRITE_WORD(idx, ch)		\
	{ vga_mapping[idx] = ((uint16_t)ch | VGA_ATTRIB); }

/** Pointer to VGA mapping. */
static uint16_t *vga_mapping = (uint16_t *)VGA_MEM_PHYS;

/** VGA cursor position variables. */
static int vga_x = 0;		/**< X position of the cursor. */
static int vga_y = 0;		/**< Y position of the cursor. */

/** Scroll the VGA console. */
static void vga_console_scroll(void) {
	int i;

	memmove(vga_mapping, vga_mapping + VGA_CONSOLE_COLS, (VGA_CONSOLE_ROWS - 1) * VGA_CONSOLE_COLS * 2);

	for(i = 0; i < VGA_CONSOLE_COLS; i++) {
		VGA_WRITE_WORD(((VGA_CONSOLE_ROWS - 1) * VGA_CONSOLE_COLS) + i, ' ');
	}

	vga_y = VGA_CONSOLE_ROWS - 1;
}

/** Move the VGA cursor. */
static void vga_console_move_csr(void) {
	out8(VGA_CRTC_INDEX, 14);
	out8(VGA_CRTC_DATA, ((vga_y * VGA_CONSOLE_COLS) + vga_x) >> 8);
	out8(VGA_CRTC_INDEX, 15);
	out8(VGA_CRTC_DATA, (vga_y * VGA_CONSOLE_COLS) + vga_x);
}

/** Write a character to the VGA console.
 * @param ch		Character to print. */
static void vga_console_putch(unsigned char ch) {
	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(vga_x != 0) {
			vga_x--;
		} else {
			vga_x = VGA_CONSOLE_COLS - 1;
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
		vga_x += 8 - (vga_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		VGA_WRITE_WORD((vga_y * VGA_CONSOLE_COLS) + vga_x, ch);
		vga_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(vga_x >= VGA_CONSOLE_COLS) {
		vga_x = 0;
		vga_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(vga_y >= VGA_CONSOLE_ROWS) {
		vga_console_scroll();
	}

	/* Move the hardware cursor to the new position. */
	vga_console_move_csr();
}

/** Initialize the VGA console. */
static void vga_console_init(void) {
	int i, j;

	/* Clear console. */
	for(i = 0; i < VGA_CONSOLE_ROWS; i++) {
		for(j = 0; j < VGA_CONSOLE_COLS; j++) {
			VGA_WRITE_WORD((i * VGA_CONSOLE_COLS) + j, ' ');
		}
	}

	/* Update X/Y psoition and move the hardware cursor. */
	vga_x = 0;
	vga_y = 0;
	vga_console_move_csr();
}

/** VGA console operations structure. */
static console_t vga_console = {
	.min_level = LOG_NORMAL,
	.putch = vga_console_putch,
	.init = vga_console_init,
};

#if 0
# pragma mark Serial console functions.
#endif

#ifdef SERIAL_PORT
/** Print a character to the serial console.
 * @param ch		Character to print. */
static void serial_console_putch(unsigned char ch) {
	while(!(in8(SERIAL_PORT + 5) & 0x20));
	if(ch == '\n') {
		serial_console_putch('\r');
	}
	out8(SERIAL_PORT, ch);
}

/** Initialize the serial console. */
static void serial_console_init(void) {
        out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
        out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
        out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
        out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
        out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
        out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
        out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

/** Serial port console. */
static console_t serial_console = {
#if CONFIG_DEBUG
	.min_level = LOG_DEBUG,
#else
	.min_level = LOG_NORMAL,
#endif
	.init = serial_console_init,
	.putch = serial_console_putch,
};
#endif

#if 0
# pragma mark -
#endif

/** Set up the console. */
void __init_text console_early_init(void) {
#ifdef SERIAL_PORT
	uint8_t status = in8(SERIAL_PORT + 6);

	/* Only add the serial device when it is present. */
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		console_register(&serial_console);
	}
#endif
	console_register(&vga_console);
}

/** Relocate the VGA memory mapping. */
void __init_text console_late_init(void) {
	void *addr;

	addr = page_phys_map(VGA_MEM_PHYS, VGA_CONSOLE_COLS * VGA_CONSOLE_ROWS * 2, 0);
	if(addr == NULL) {
		fatal("Could not map VGA memory");
	}

	vga_mapping = (uint16_t *)addr;
}
