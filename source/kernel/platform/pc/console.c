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

/** VGA registers to apply for 80x50 text mode. */
static unsigned char vga_regs_80x50[VGA_NUM_REGS] = {
	/* MISC. */
	0x67,

	/* SEQ. */
	0x03, 0x00, 0x03, 0x00, 0x02,

	/* CRTC. */
	0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x47, 0x06, 0x07,
	0x00, 0x00, 0x01, 0x40, 0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
	0xFF,

	/* GC. */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,

	/* AC. */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F, 0x08, 0x00,
};

/** Set the plane we're reading and writing to/from. */
static void vga_set_plane(uint32_t p) {
	unsigned char pmask;

	p &= 3;
	pmask = 1 << p;

	/* Set read plane, then write plane. */
	out8(VGA_GC_INDEX, 4);
	out8(VGA_GC_DATA, p);
	out8(VGA_SEQ_INDEX, 2);
	out8(VGA_SEQ_DATA, pmask);
}

/** Change the console font. */
static void vga_write_font(unsigned char *buf, unsigned int height) {
	unsigned char seq2, seq4, gc4, gc5, gc6;
	uint32_t i;

	/* Save registers that we're going to modify - vga_set_plane()
	 * modifies GC 4 and SEQ 2, so they must also be saved. */
	out8(VGA_SEQ_INDEX, 2);
	seq2 = in8(VGA_SEQ_DATA);
	out8(VGA_SEQ_INDEX, 4);
	seq4 = in8(VGA_SEQ_DATA);
	out8(VGA_GC_INDEX, 4);
	gc4 = in8(VGA_GC_DATA);
	out8(VGA_GC_INDEX, 5);
	gc5 = in8(VGA_GC_DATA);
	out8(VGA_GC_INDEX, 6);
	gc6 = in8(VGA_GC_DATA);

	/* Turn off even-odd addressing (set flat addressing). */
	out8(VGA_SEQ_INDEX, 4);
	out8(VGA_SEQ_DATA, seq4 | 0x04);
	out8(VGA_GC_INDEX, 5);
	out8(VGA_GC_DATA, gc5 & ~0x10);
	out8(VGA_GC_INDEX, 6);
	out8(VGA_GC_DATA, gc6 & ~0x02);

	/* Write font to plane P4. */
	vga_set_plane(2);

	/* Write the font. */
	for(i = 0; i < 256; i++) {
		memcpy((char *)((ptr_t)vga_mapping + (i * 32)), &buf[i * height], height);
	}

	/* Restore modified registers. */
	out8(VGA_SEQ_INDEX, 2);
	out8(VGA_SEQ_DATA, seq2);
	out8(VGA_SEQ_INDEX, 4);
	out8(VGA_SEQ_DATA, seq4);
	out8(VGA_GC_INDEX, 4);
	out8(VGA_GC_DATA, gc4);
	out8(VGA_GC_INDEX, 5);
	out8(VGA_GC_DATA, gc5);
	out8(VGA_GC_INDEX, 6);
	out8(VGA_GC_DATA, gc6);
}

/** Write a VGA register array to the VGA registers. */
static void vga_write_regs(unsigned char *regs) {
	uint32_t i, off = 0;

	/* Write MISC register. */
	out8(VGA_MISC_WRITE, regs[off++]);

	/* Write SEQ registers. */
	for(i = 0; i < VGA_NUM_SEQ_REGS; i++) {
		out8(VGA_SEQ_INDEX, i);
		out8(VGA_SEQ_DATA, regs[off++]);
	}

	/* Unlock CRTC registers. */
	out8(VGA_CRTC_INDEX, 0x03);
	out8(VGA_CRTC_DATA, in8(VGA_CRTC_DATA) | 0x80);
	out8(VGA_CRTC_INDEX, 0x11);
	out8(VGA_CRTC_DATA, in8(VGA_CRTC_DATA) & ~0x80);

	/* Write CRTC registers. */
	for(i = 0; i < VGA_NUM_CRTC_REGS; i++) {
		out8(VGA_CRTC_INDEX, i);
		out8(VGA_CRTC_DATA, regs[off++]);
	}

	/* Write GC registers. */
	for(i = 0; i < VGA_NUM_GC_REGS; i++) {
		out8(VGA_GC_INDEX, i);
		out8(VGA_GC_DATA, regs[off++]);
	}

	/* Write AC registers. */
	for(i = 0; i < VGA_NUM_AC_REGS; i++) {
		in8(VGA_INSTAT_READ);
		out8(VGA_AC_INDEX, i);
		out8(VGA_AC_WRITE, regs[off++]);
	}

	/* Lock 16-color palette and unblank display. */
	in8(VGA_INSTAT_READ);
	out8(VGA_AC_INDEX, 0x20);
}

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

/** Initialise the VGA console. */
static void vga_console_init(void) {
	int i, j;

	vga_write_regs(vga_regs_80x50);
	vga_write_font(console_font_8x8, 8);

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

/** Initialise the serial console. */
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
