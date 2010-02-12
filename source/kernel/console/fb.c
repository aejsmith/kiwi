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
 * @brief		Framebuffer console.
 */

#include <lib/ctype.h>
#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <sync/spinlock.h>

#include <console.h>
#include <errors.h>
#include <kargs.h>

extern unsigned char g_console_font[];
extern unsigned char g_logo_ppm[];
extern unsigned char g_copyright_ppm[];

/** Width and height of the console font. */
#define FONT_WIDTH		6
#define FONT_HEIGHT		12

/** Colour and size of the splash progress bar. */
#define SPLASH_PROGRESS_FG	0x78cc00
#define SPLASH_PROGRESS_BG	0x555555
#define SPLASH_PROGRESS_WIDTH	300
#define SPLASH_PROGRESS_HEIGHT	3

/** Size of one row in bytes. */
#define ROW_SIZE		((g_fb_console_width * FONT_HEIGHT) * (g_fb_console_depth / 8))

/** Get red colour from RGB value. */
#define RED(x, bits)		((x >> (24 - bits)) & ((1 << bits) - 1))

/** Get green colour from RGB value. */
#define GREEN(x, bits)		((x >> (16 - bits)) & ((1 << bits) - 1))

/** Get blue colour from RGB value. */
#define BLUE(x, bits)		((x >> (8  - bits)) & ((1 << bits) - 1))

/** Boot splash information. */
static bool g_splash_enabled = false;
static uint16_t g_splash_progress_x;
static uint16_t g_splash_progress_y;

/** Framebuffer console details. */
static uint16_t g_fb_console_width;
static uint16_t g_fb_console_height;
static uint8_t g_fb_console_depth;
static uint16_t g_fb_console_cols;
static uint16_t g_fb_console_rows;
static uint16_t g_fb_console_x = 0;
static uint16_t g_fb_console_y = 0;
static char *g_fb_console_mapping = NULL;

/** Backbuffer for the console. */
static char *g_fb_console_buffer = NULL;

/** Lock for the framebuffer console. */
static SPINLOCK_DECLARE(fb_console_lock);

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
	size_t offset = ((y * g_fb_console_width) + x) * (g_fb_console_depth / 8);
	char *mdest = g_fb_console_mapping + offset;
	char *bdest = g_fb_console_buffer + offset;
	uint16_t colour16;

	/* Draw the pixel. */
	switch(g_fb_console_depth) {
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

	spinlock_lock(&fb_console_lock, 0);

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(g_fb_console_x) {
			g_fb_console_x--;
		} else if(g_fb_console_y) {
			g_fb_console_x = g_fb_console_rows - 1;
			g_fb_console_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		g_fb_console_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		g_fb_console_x = 0;
		g_fb_console_y++;
		break;
	case '\t':
		g_fb_console_x += 8 - (g_fb_console_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		x = g_fb_console_x * FONT_WIDTH;
		y = g_fb_console_y * FONT_HEIGHT;
		for(i = 0; i < FONT_HEIGHT; i++) {
			for(j = 0; j < FONT_WIDTH; j++) {
				if(g_console_font[(ch * FONT_HEIGHT) + i] & (1<<(7-j))) {
					fb_console_putpixel(0xffffff, x + j, y + i);
				} else {
					fb_console_putpixel(0x0, x + j, y + i);
				}
			}
		}

		g_fb_console_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(g_fb_console_x >= g_fb_console_cols) {
		g_fb_console_x = 0;
		g_fb_console_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(g_fb_console_y >= g_fb_console_rows) {
		/* Move everything up in the backbuffer, and fill the last row
		 * with blanks. */
		memcpy(g_fb_console_buffer, g_fb_console_buffer + ROW_SIZE,
		       ROW_SIZE * (g_fb_console_rows - 1));
		memset(g_fb_console_buffer + (ROW_SIZE * (g_fb_console_rows - 1)), 0, ROW_SIZE);

		/* Copy the updated backbuffer onto the framebuffer. */
		memcpy(g_fb_console_mapping, g_fb_console_buffer,
		       g_fb_console_width * g_fb_console_height * (g_fb_console_depth / 8));

		/* Update the cursor position. */
		g_fb_console_y = g_fb_console_rows - 1;
	}

	spinlock_unlock(&fb_console_lock);
}

/** Framebuffer console. */
console_t g_fb_console = {
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
	nmap = page_phys_map(addr, width * height * (depth / 8), MM_SLEEP);
	memset(nmap, 0, width * height * (depth / 8));
	nbuf = kcalloc(width * height, depth / 8, MM_SLEEP);

	/* Take the lock across updating the details (must be done in case
	 * another CPU tries to write to the console while updating). */
	spinlock_lock(&fb_console_lock, 0);

	/* Save old mapping/buffer details to unmap/free after unlocking. */
	if(g_fb_console_mapping) {
		omap = g_fb_console_mapping;
		obuf = g_fb_console_buffer;
		size = g_fb_console_width * g_fb_console_height * (g_fb_console_depth / 8);
	}

	/* Store new details. */
	g_fb_console_mapping = nmap;
	g_fb_console_buffer = nbuf;
	g_fb_console_width = width;
	g_fb_console_height = height;
	g_fb_console_depth = depth;
	g_fb_console_cols = width / FONT_WIDTH;
	g_fb_console_rows = height / FONT_HEIGHT;
	g_fb_console_x = g_fb_console_y = 0;

	spinlock_unlock(&fb_console_lock);

	/* Free the old mapping/buffer if necessary. */
	if(omap) {
		page_phys_unmap(omap, size, true);
		kfree(obuf);
	}
}

/** Reset the framebuffer console cursor position. */
void fb_console_reset(void) {
	spinlock_lock(&fb_console_lock, 0);
	g_fb_console_x = g_fb_console_y = 0;
	spinlock_unlock(&fb_console_lock);
}

/** Initialise the framebuffer console.
 * @param args		Kernel arguments. */
void __init_text console_init(kernel_args_t *args) {
	uint16_t width, height;

	/* Configure the console using information from the bootloader and
	 * register it. */
	fb_console_reconfigure(args->fb_width, args->fb_height, args->fb_depth, args->fb_addr);
	console_register(&g_fb_console);

	if(!args->splash_disabled) {
		g_fb_console.inhibited = true;
		g_splash_enabled = true;

		/* Draw copyright text. */
		ppm_size(g_copyright_ppm, &width, &height);
		fb_console_draw_ppm(g_copyright_ppm, (g_fb_console_width / 2) - (width / 2),
		                    g_fb_console_height - height - 5);

		/* Get logo dimensions. */
		ppm_size(g_logo_ppm, &width, &height);

		/* Determine where to draw the progress bar. */
		g_splash_progress_x = (g_fb_console_width / 2) - (SPLASH_PROGRESS_WIDTH / 2);
		g_splash_progress_y = (g_fb_console_height / 2) + (height / 2) + 10;

		/* Draw logo. */
		fb_console_draw_ppm(g_logo_ppm, (g_fb_console_width / 2) - (width / 2),
		                    (g_fb_console_height / 2) - (height / 2) - 10);

		/* Draw initial progress bar. */
		console_update_boot_progress(0);
	}
}

/** Update the progress on the boot splash.
 * @param percent	Boot progress percentage. */
void console_update_boot_progress(int percent) {
	if(g_splash_enabled) {
		fb_console_fillrect(SPLASH_PROGRESS_BG, g_splash_progress_x,
		                    g_splash_progress_y, SPLASH_PROGRESS_WIDTH,
		                    SPLASH_PROGRESS_HEIGHT);
		fb_console_fillrect(SPLASH_PROGRESS_FG, g_splash_progress_x,
		                    g_splash_progress_y, (SPLASH_PROGRESS_WIDTH * percent) / 100,
		                    SPLASH_PROGRESS_HEIGHT);
	}
}
