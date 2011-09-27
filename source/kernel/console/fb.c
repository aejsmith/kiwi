/*
 * Copyright (C) 2010-2011 Alex Smith
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
#include <mm/phys.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>

#if CONFIG_DEBUG
KBOOT_BOOLEAN_OPTION("splash_disabled", "Disable splash screen", true);
#else
KBOOT_BOOLEAN_OPTION("splash_disabled", "Disable splash screen", false);
#endif

extern unsigned char console_font[];
extern unsigned char logo_ppm[];
extern unsigned char copyright_ppm[];

/** Width and height of the console font. */
#define FONT_WIDTH		6
#define FONT_HEIGHT		12

/** Colour and size of the splash progress bar. */
#define SPLASH_PROGRESS_FG	0x78cc00
#define SPLASH_PROGRESS_BG	0x555555
#define SPLASH_PROGRESS_WIDTH	250
#define SPLASH_PROGRESS_HEIGHT	3

/** Size of one row in bytes. */
#define ROW_SIZE		((fb_console_mode.width * FONT_HEIGHT) * (fb_console_mode.depth / 8))

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
static fb_info_t fb_console_mode;
static char *fb_console_mapping = NULL;

/** Backbuffer for the console. */
static char *fb_console_buffer = NULL;

/** Framebuffer console details. */
static bool fb_console_acquired = false;
static uint16_t fb_console_cols;
static uint16_t fb_console_rows;
static uint16_t fb_console_x = 0;
static uint16_t fb_console_y = 0;

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
	size_t offset = ((y * fb_console_mode.width) + x) * (fb_console_mode.depth / 8);
	char *mdest = fb_console_mapping + offset;
	char *bdest = fb_console_buffer + offset;
	uint16_t colour16;

	/* Draw the pixel. */
	switch(fb_console_mode.depth) {
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
static void fb_console_putc(unsigned char ch) {
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
		       fb_console_mode.width * fb_console_mode.height * (fb_console_mode.depth / 8));

		/* Update the cursor position. */
		fb_console_y = fb_console_rows - 1;
	}
}

/** Framebuffer console. */
static console_t fb_console = {
	.min_level = LOG_NOTICE,
	.putc = fb_console_putc,
};

/** Reconfigure the framebuffer console.
 * @param info		Information structure for new framebuffer. */
static void fb_console_configure(const fb_info_t *info) {
	void *nmap, *nbuf, *omap = NULL, *obuf = NULL;
	size_t size = 0;

	/* Map the new framebuffer, and allocate a backbuffer. */
	nmap = phys_map(info->addr, info->width * info->height * (info->depth / 8), MM_SLEEP);
	nbuf = kcalloc(info->width * info->height, info->depth / 8, MM_SLEEP);

	/* Save old mapping/buffer details to unmap/free after unlocking. */
	if(fb_console_mapping) {
		omap = fb_console_mapping;
		obuf = fb_console_buffer;
		size = fb_console_mode.width * fb_console_mode.height * (fb_console_mode.depth / 8);
	}

	/* Store new details. */
	fb_console_mapping = nmap;
	fb_console_buffer = nbuf;
	memcpy(&fb_console_mode, info, sizeof(*info));
	fb_console_cols = info->width / FONT_WIDTH;
	fb_console_rows = info->height / FONT_HEIGHT;
	fb_console_x = fb_console_y = 0;

	/* Free the old mapping/buffer if necessary. */
	if(omap) {
		phys_unmap(omap, size, true);
		kfree(obuf);
	}
}

/** Reset the framebuffer console. */
static void fb_console_reset(void) {
	/* Reset the cursor position and clear the first row of the console. */
	fb_console_x = fb_console_y = 0;
	memset(fb_console_mapping, 0, ROW_SIZE);
	memset(fb_console_buffer, 0, ROW_SIZE);
}

/** Enable the framebuffer console upon KDB entry/fatal().
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void fb_console_enable(void *arg1, void *arg2, void *arg3) {
	if(!fb_console.putc) {
		fb_console.putc = fb_console_putc;
		fb_console_reset();
	}
}

/** Disable the framebuffer console upon KDB exit.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void fb_console_disable(void *arg1, void *arg2, void *arg3) {
	if(fb_console_acquired || splash_enabled) {
		fb_console.putc = NULL;
	}
}

/** Control the framebuffer console.
 * @param op		Operation to perform.
 * @param info		For FB_CONSOLE_INFO and FB_CONSOLE_CONFIGURE, FB info
 *			structure to use. */
void fb_console_control(unsigned op, fb_info_t *info) {
	switch(op) {
	case FB_CONSOLE_INFO:
		/* Get information on the current mode. */
		memcpy(info, &fb_console_mode, sizeof(*info));
		break;
	case FB_CONSOLE_CONFIGURE:
		/* Reconfigure to use a new framebuffer. */
		fb_console_configure(info);
		break;
	case FB_CONSOLE_ACQUIRE:
		/* Acquire the framebuffer for exclusive use. */
		if(fb_console_acquired) {
			fatal("Framebuffer console already acquired");
		}

		/* Prevent output to the console. */
		fb_console.putc = NULL;
		fb_console_acquired = true;
		break;
	case FB_CONSOLE_RELEASE:
		/* Release the framebuffer. */
		if(!fb_console_acquired) {
			fatal("Framebuffer console not acquired");
		}

		/* If the splash wasn't enabled, re-enable console output. */
		if(!splash_enabled) {
			fb_console.putc = fb_console_putc;
			fb_console_reset();
		}

		fb_console_acquired = false;
		break;
	};
}

/** Initialise the framebuffer console. */
__init_text void console_init(void) {
	uint16_t width, height;
	kboot_tag_lfb_t *lfb;
	fb_info_t info;

	/* Do platform initialisation. */
	platform_console_init();

	/* Look up the KBoot framebuffer tag. */
	lfb = kboot_tag_iterate(KBOOT_TAG_LFB, NULL);
	if(!lfb) {
		kprintf(LOG_WARN, "console: no framebuffer information provided by loader\n");
		return;
	}

	/* Configure the framebuffer. */
	info.width = lfb->width;
	info.height = lfb->height;
	info.depth = lfb->depth;
	info.addr = lfb->addr;
	kboot_tag_release(lfb);
	fb_console_configure(&info);

	/* Clear the framebuffer and register the console. */
	memset(fb_console_mapping, 0, info.width * info.height * (info.depth / 8));
	console_register(&fb_console);

	/* Register callbacks to reset the framebuffer console upon fatal() and
	 * KDB entry. */
	notifier_register(&fatal_notifier, fb_console_enable, NULL);
	notifier_register(&kdb_entry_notifier, fb_console_enable, NULL);
	notifier_register(&kdb_exit_notifier, fb_console_disable, NULL);

	if(!kboot_boolean_option("splash_disabled")) {
		splash_enabled = true;

		/* Prevent log output. */
		fb_console.putc = NULL;

		/* Draw copyright text. */
		ppm_size(copyright_ppm, &width, &height);
		fb_console_draw_ppm(copyright_ppm, (fb_console_mode.width / 2) - (width / 2),
		                    fb_console_mode.height - height - 5);

		/* Get logo dimensions. */
		ppm_size(logo_ppm, &width, &height);

		/* Determine where to draw the progress bar. */
		splash_progress_x = (fb_console_mode.width / 2) - (SPLASH_PROGRESS_WIDTH / 2);
		splash_progress_y = (fb_console_mode.height / 2) + (height / 2) + 20;

		/* Draw logo. */
		fb_console_draw_ppm(logo_ppm, (fb_console_mode.width / 2) - (width / 2),
		                    (fb_console_mode.height / 2) - (height / 2) - 10);

		/* Draw initial progress bar. */
		console_update_boot_progress(0);
	}
}

/** Update the progress on the boot splash.
 * @param percent	Boot progress percentage. */
void console_update_boot_progress(int percent) {
	if(splash_enabled && !fb_console_acquired) {
		fb_console_fillrect(SPLASH_PROGRESS_BG, splash_progress_x,
		                    splash_progress_y, SPLASH_PROGRESS_WIDTH,
		                    SPLASH_PROGRESS_HEIGHT);
		fb_console_fillrect(SPLASH_PROGRESS_FG, splash_progress_x,
		                    splash_progress_y, (SPLASH_PROGRESS_WIDTH * percent) / 100,
		                    SPLASH_PROGRESS_HEIGHT);
	}
}
