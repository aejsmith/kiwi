/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Framebuffer console.
 */

#include <lib/ctype.h>
#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kargs.h>

extern unsigned char console_font[];
extern unsigned char logo_ppm[];
extern unsigned char copyright_ppm[];

/** Width and height of the console font. */
#define FONT_WIDTH		6
#define FONT_HEIGHT		11

/** Colour and size of the splash progress bar. */
#define SPLASH_PROGRESS_FG	0x78cc00
#define SPLASH_PROGRESS_BG	0x555555
#define SPLASH_PROGRESS_WIDTH	250
#define SPLASH_PROGRESS_HEIGHT	3

/** Size of one row in bytes. */
#define ROW_SIZE		((fb_console_width * FONT_HEIGHT) * (fb_console_depth / 8))

/** Get red colour from RGB value. */
#define RED(x, bits)		((x >> (24 - bits)) & ((1 << bits) - 1))

/** Get green colour from RGB value. */
#define GREEN(x, bits)		((x >> (16 - bits)) & ((1 << bits) - 1))

/** Get blue colour from RGB value. */
#define BLUE(x, bits)		((x >> (8  - bits)) & ((1 << bits) - 1))

/** Boot splash information. */
static bool splash_enabled = false;
static uint16_t splash_progress_x;
static uint16_t splash_progress_y;

/** Current display mode. */
uint16_t fb_console_width;
uint16_t fb_console_height;
uint8_t fb_console_depth;

/** Framebuffer console details. */
static uint16_t fb_console_cols;
static uint16_t fb_console_rows;
static uint16_t fb_console_x = 0;
static uint16_t fb_console_y = 0;
static char *fb_console_mapping = NULL;

/** Backbuffer for the console. */
static char *fb_console_buffer = NULL;

/* Skip over whitespace and comments in a PPM file.
 * @param buf		Pointer to data buffer.
 * @return		Address of next non-whitespace byte. */
static unsigned char *ppm_skip(unsigned char *buf) {
	while(true) {
		while(isspace(*buf)) {
			buf++;
		}

		if(*buf == '#') {
			while(*buf != '\n' && *buf != '\r') {
				buf++;
			}
		} else {
			break;
		}
	}

	return buf;
}

/** Get the dimensions of a PPM image.
 * @param ppm		Buffer containing PPM image.
 * @param widthp	Where to store image width.
 * @param heightp	Where to store image height. */
static void ppm_size(unsigned char *ppm, uint16_t *widthp, uint16_t *heightp) {
	if((ppm[0] != 'P') || (ppm[1] != '6')) {
		*widthp = 0;
		*heightp = 0;
		return;
	}
	ppm += 2;

	ppm = ppm_skip(ppm);
	*widthp = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm = ppm_skip(ppm);
	*heightp = strtoul((const char *)ppm, (char **)&ppm, 10);
}

/** Draw a pixel to the framebuffer.
 * @param colour	RGB colour for pixel.
 * @param x		X position of pixel.
 * @param y		Y position of pixel. */
static void fb_console_putpixel(uint32_t colour, uint16_t x, uint16_t y) {
	size_t offset = ((y * fb_console_width) + x) * (fb_console_depth / 8);
	char *mdest = fb_console_mapping + offset;
	char *bdest = fb_console_buffer + offset;
	uint16_t colour16;

	/* Draw the pixel. */
	switch(fb_console_depth) {
	case 15:
		colour16 = (RED(colour, 5) << 11) | (GREEN(colour, 5) << 5) | BLUE(colour, 5);
		*(uint16_t *)mdest = *(uint16_t *)bdest = colour16;
		break;
	case 16:
		colour16 = (RED(colour, 5) << 11) | (GREEN(colour, 6) << 5) | BLUE(colour, 5);
		*(uint16_t *)mdest = *(uint16_t *)bdest = colour16;
		break;
	case 24:
		colour &= 0x00FFFFFF;
	case 32:
		*(uint32_t *)mdest = *(uint32_t *)bdest = colour;
		break;
	}
}

/** Draw a PPM image on the framebuffer.
 * @param ppm		Buffer containing PPM image.
 * @param x		X position of image.
 * @param y		Y position of image. */
static void fb_console_draw_ppm(unsigned char *ppm, uint16_t x, uint16_t y) {
	uint32_t max_colour, coef, colour;
	uint16_t width, height, i, j;

	if((ppm[0] != 'P') || (ppm[1] != '6')) {
		return;
	}
	ppm += 2;

	ppm = ppm_skip(ppm);
	width = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm = ppm_skip(ppm);
	height = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm = ppm_skip(ppm);
	max_colour = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm++;

	if(!max_colour || max_colour > 255) {
		return;
	}

	coef = 255 / max_colour;
	if((coef * max_colour) > 255) {
		coef -= 1;
	}

	/* Draw the image. */
	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			colour = 0;
			colour |= (*(ppm++) * coef) << 16;
			colour |= (*(ppm++) * coef) << 8;
			colour |= *(ppm++) * coef;
			fb_console_putpixel(colour, x + j, y + i);
		}
	}
}

/** Draw a rectangle in a solid colour.
 * @param colour	Colour to draw in.
 * @param x		X position of rectangle.
 * @param y		Y position of rectangle.
 * @param width		Width of rectangle.
 * @param height	Height of rectangle. */
static void fb_console_fillrect(uint32_t colour, uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height) {
	uint16_t i, j;

	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			fb_console_putpixel(colour, x + j, y + i);
		}
	}
}

/** Write a character to the console.
 * @param ch		Character to write. */
static void fb_console_putch(unsigned char ch) {
	int i, j, x, y;

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(fb_console_x) {
			fb_console_x--;
		} else if(fb_console_y) {
			fb_console_x = fb_console_rows - 1;
			fb_console_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		fb_console_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was there (will
		 * be handled below. */
		fb_console_x = fb_console_cols;
		break;
	case '\t':
		fb_console_x += 8 - (fb_console_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		x = fb_console_x * FONT_WIDTH;
		y = fb_console_y * FONT_HEIGHT;
		for(i = 0; i < FONT_HEIGHT; i++) {
			for(j = 0; j < FONT_WIDTH; j++) {
				if(console_font[(ch * FONT_HEIGHT) + i] & (1<<(7-j))) {
					fb_console_putpixel(0xffffff, x + j, y + i);
				} else {
					fb_console_putpixel(0x0, x + j, y + i);
				}
			}
		}

		fb_console_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(fb_console_x >= fb_console_cols) {
		fb_console_x = 0;
		if(++fb_console_y < fb_console_rows) {
			memset(fb_console_mapping + (ROW_SIZE * fb_console_y), 0, ROW_SIZE);
			memset(fb_console_buffer + (ROW_SIZE * fb_console_y), 0, ROW_SIZE);
		}
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(fb_console_y >= fb_console_rows) {
		/* Move everything up in the backbuffer, and fill the last row
		 * with blanks. */
		memcpy(fb_console_buffer, fb_console_buffer + ROW_SIZE,
		       ROW_SIZE * (fb_console_rows - 1));
		memset(fb_console_buffer + (ROW_SIZE * (fb_console_rows - 1)), 0, ROW_SIZE);

		/* Copy the updated backbuffer onto the framebuffer. */
		memcpy(fb_console_mapping, fb_console_buffer,
		       fb_console_width * fb_console_height * (fb_console_depth / 8));

		/* Update the cursor position. */
		fb_console_y = fb_console_rows - 1;
	}
}

/** Framebuffer console. */
console_t fb_console = {
	.min_level = LOG_NORMAL,
	.putch = fb_console_putch,
};

/** Reconfigure the framebuffer console.
 * @param width		Width of display.
 * @param height	Height of display.
 * @param depth		Bits per pixel.
 * @param addr		Physical address of framebuffer. */
void fb_console_reconfigure(uint16_t width, uint16_t height, uint8_t depth, phys_ptr_t addr) {
	void *nmap, *nbuf, *omap = NULL, *obuf = NULL;
	size_t size = 0;

	/* Map and clear the new framebuffer, and allocate a backbuffer. */
	nmap = phys_map(addr, width * height * (depth / 8), MM_SLEEP);
	if(!fb_console.inhibited) {
		memset(nmap, 0, width * height * (depth / 8));
	}
	nbuf = kcalloc(width * height, depth / 8, MM_SLEEP);

	/* Take the lock across updating the details (must be done in case
	 * another CPU tries to write to the console while updating). */
	// FIXME
	//spinlock_lock(&fb_console_lock, 0);

	/* Save old mapping/buffer details to unmap/free after unlocking. */
	if(fb_console_mapping) {
		omap = fb_console_mapping;
		obuf = fb_console_buffer;
		size = fb_console_width * fb_console_height * (fb_console_depth / 8);
	}

	/* Store new details. */
	fb_console_mapping = nmap;
	fb_console_buffer = nbuf;
	fb_console_width = width;
	fb_console_height = height;
	fb_console_depth = depth;
	fb_console_cols = width / FONT_WIDTH;
	fb_console_rows = height / FONT_HEIGHT;
	fb_console_x = fb_console_y = 0;

	/* Disable the splash screen so that the new framebuffer doesn't end
	 * up with a progress bar on it if something in userspace tries to
	 * update the boot progress. */
	splash_enabled = false;

	//spinlock_unlock(&fb_console_lock);

	/* Free the old mapping/buffer if necessary. */
	if(omap) {
		phys_unmap(omap, size, true);
		kfree(obuf);
	}
}

/** Reset the framebuffer console cursor position. */
void fb_console_reset(void) {
	//spinlock_lock(&fb_console_lock, 0);
	fb_console_x = fb_console_y = 0;
	memset(fb_console_mapping, 0, ROW_SIZE);
	memset(fb_console_buffer, 0, ROW_SIZE);
	//spinlock_unlock(&fb_console_lock);
}

/** Initialise the framebuffer console.
 * @param args		Kernel arguments. */
void __init_text console_init(kernel_args_t *args) {
	uint16_t width, height;

	/* Configure the console using information from the bootloader and
	 * register it. */
	fb_console_reconfigure(args->fb_width, args->fb_height, args->fb_depth, args->fb_addr);
	console_register(&fb_console);

	if(!args->splash_disabled) {
		fb_console.inhibited = true;
		splash_enabled = true;

		/* Draw copyright text. */
		ppm_size(copyright_ppm, &width, &height);
		fb_console_draw_ppm(copyright_ppm, (fb_console_width / 2) - (width / 2),
		                    fb_console_height - height - 5);

		/* Get logo dimensions. */
		ppm_size(logo_ppm, &width, &height);

		/* Determine where to draw the progress bar. */
		splash_progress_x = (fb_console_width / 2) - (SPLASH_PROGRESS_WIDTH / 2);
		splash_progress_y = (fb_console_height / 2) + (height / 2) + 20;

		/* Draw logo. */
		fb_console_draw_ppm(logo_ppm, (fb_console_width / 2) - (width / 2),
		                    (fb_console_height / 2) - (height / 2) - 10);

		/* Draw initial progress bar. */
		console_update_boot_progress(0);
	}
}

/** Update the progress on the boot splash.
 * @param percent	Boot progress percentage. */
void console_update_boot_progress(int percent) {
	if(splash_enabled) {
		fb_console_fillrect(SPLASH_PROGRESS_BG, splash_progress_x,
		                    splash_progress_y, SPLASH_PROGRESS_WIDTH,
		                    SPLASH_PROGRESS_HEIGHT);
		fb_console_fillrect(SPLASH_PROGRESS_FG, splash_progress_x,
		                    splash_progress_y, (SPLASH_PROGRESS_WIDTH * percent) / 100,
		                    SPLASH_PROGRESS_HEIGHT);
	}
}
