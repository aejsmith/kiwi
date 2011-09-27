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
 *
 * @todo		This does not handle endianness very well. In fact the
 *			rendering code is generally a little bit shit.
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

/** Width, height and colours of the console font. */
#define FONT_WIDTH		6
#define FONT_HEIGHT		12
#define FONT_FG			0xffffff
#define FONT_BG			0x000000

/** Colour and size of the splash progress bar. */
#define SPLASH_PROGRESS_FG	0x78cc00
#define SPLASH_PROGRESS_BG	0x555555
#define SPLASH_PROGRESS_WIDTH	250
#define SPLASH_PROGRESS_HEIGHT	3

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

/** Get the byte offset of a pixel in the framebuffer.
 * @param x		X position of pixel.
 * @param y		Y position of pixel.
 * @return		Byte offset of pixel in framebuffer. */
static inline size_t fb_console_offset(uint16_t x, uint16_t y) {
	return (((y * fb_console_mode.width) + x) * (fb_console_mode.depth / 8));
}

/** Get the RGB value of a pixel.
 * @param x		X position of pixel.
 * @param y		Y position of pixel.
 * @return		Value of the pixel. */
static uint32_t fb_console_getpixel(uint16_t x, uint16_t y) {
	uint8_t *src = (uint8_t *)(fb_console_buffer + fb_console_offset(x, y));
	uint32_t ret = 0;
	uint16_t val16;

	switch(fb_console_mode.depth) {
	case 16:
		val16 = *(uint16_t *)src;
		ret |= (((val16 >> 11) & 0x1f) << (16 + 3));
		ret |= (((val16 >> 5) & 0x3f) << (8 + 2));
		ret |= ((val16 & 0x1f) << 3);
		break;
	case 24:
		ret = (src[0] << 16) | (src[1] << 8) | src[2];
		break;
	case 32:
		ret = *(uint32_t *)src;
		break;
	}

	return ret;
}

/** Draw a pixel to the framebuffer.
 * @param x		X position of pixel.
 * @param y		Y position of pixel.
 * @param rgb		RGB colour for pixel. */
static void fb_console_putpixel(uint16_t x, uint16_t y, uint32_t rgb) {
	char *mdest = fb_console_mapping + fb_console_offset(x, y);
	char *bdest = fb_console_buffer + fb_console_offset(x, y);

	switch(fb_console_mode.depth) {
	case 16:
		*(uint16_t *)mdest = *(uint16_t *)bdest = (RED(rgb, 5) << 11) | (GREEN(rgb, 6) << 5) | BLUE(rgb, 5);
		break;
	case 24:
		((uint8_t *)mdest)[0] = ((uint8_t *)bdest)[0] = RED(rgb, 8);
		((uint8_t *)mdest)[1] = ((uint8_t *)bdest)[1] = GREEN(rgb, 8);
		((uint8_t *)mdest)[2] = ((uint8_t *)bdest)[2] = BLUE(rgb, 8);
		break;
	case 32:
		*(uint32_t *)mdest = *(uint32_t *)bdest = rgb;
		break;
	}
}

/** Compare a pixel on the framebuffer.
 * @param x		X position of pixel.
 * @param y		Y position of pixel.
 * @param cmp		RGB colour to compare with.
 * @return		Whether the pixels are equal. */
static bool fb_console_cmppixel(uint16_t x, uint16_t y, uint32_t cmp) {
	uint32_t value = 0;

	/* Work out the value the pixel would be on the framebuffer. */
	switch(fb_console_mode.depth) {
	case 16:
		value = (cmp & 0xf8fcf8);
		break;
	case 24:
		value = (cmp & 0xffffff);
		break;
	case 32:
		value = cmp;
		break;
	}

	return (fb_console_getpixel(x, y) == value);
}

/** Draw a rectangle in a solid colour.
 * @param x		X position of rectangle.
 * @param y		Y position of rectangle.
 * @param width		Width of rectangle.
 * @param height	Height of rectangle.
 * @param rgb		Colour to draw in. */
static void fb_console_fillrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t rgb) {
	uint16_t i, j;

	if(x == 0 && width == fb_console_mode.width && (rgb == 0 || rgb == 0xffffff)) {
		memset(fb_console_buffer + fb_console_offset(0, y), (uint8_t)rgb,
		       width * height * (fb_console_mode.depth / 8));
		memset(fb_console_mapping + fb_console_offset(0, y), (uint8_t)rgb,
		       width * height * (fb_console_mode.depth / 8));
	} else {
		for(i = 0; i < height; i++) {
			for(j = 0; j < width; j++) {
				fb_console_putpixel(x + j, y + i, rgb);
			}
		}
	}
}

/** Copy part of the framebuffer to another location.
 * @param dest_y	Y position of destination.
 * @param src_y		Y position of source area.
 * @param height	Height of area to copy. */
static void fb_console_copyrect(uint16_t dest_y, uint16_t src_y, uint16_t height) {
	size_t dest_offset = fb_console_offset(0, dest_y);
	size_t src_offset = fb_console_offset(0, src_y);

	/* Copy everything on the backbuffer. */
	memmove(fb_console_buffer + dest_offset, fb_console_buffer + src_offset,
	        fb_console_mode.width * height * (fb_console_mode.depth / 8));

	/* Copy the updated backbuffer onto the framebuffer. */
	memcpy(fb_console_mapping + dest_offset, fb_console_buffer + dest_offset,
		fb_console_mode.width * height * (fb_console_mode.depth / 8));
}

/** Toggle the cursor. */
static void fb_console_toggle_cursor(void) {
	uint16_t i, j, x, y;

	for(i = 0; i < FONT_HEIGHT; i++) {
		for(j = 0; j < FONT_WIDTH; j++) {
			x = (fb_console_x * FONT_WIDTH) + j;
			y = (fb_console_y * FONT_HEIGHT) + i;

			/* We cannot just invert the pixel value here. On
			 * framebuffers less than 24-bit, the value we read
			 * back is not necessarily the same as the one we
			 * write in the first place. */
			if(fb_console_cmppixel(x, y, FONT_FG)) {
				fb_console_putpixel(x, y, FONT_BG);
			} else {
				fb_console_putpixel(x, y, FONT_FG);
			}
		}
	}
}

/** Write a character to the console.
 * @param ch		Character to write. */
static void fb_console_putc(unsigned char ch) {
	uint16_t i, j, x, y;

	fb_console_toggle_cursor();

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
					fb_console_putpixel(x + j, y + i, FONT_FG);
				} else {
					fb_console_putpixel(x + j, y + i, FONT_BG);
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
			fb_console_fillrect(0, FONT_HEIGHT * fb_console_y, fb_console_mode.width,
			                    FONT_HEIGHT, FONT_BG);
		}
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(fb_console_y >= fb_console_rows) {
		/* Move everything up and fill the last row with blanks. */
		fb_console_copyrect(0, FONT_HEIGHT, (fb_console_rows - 1) * FONT_HEIGHT);
		fb_console_fillrect(0, FONT_HEIGHT * (fb_console_rows - 1), fb_console_mode.width,
		                    FONT_HEIGHT, FONT_BG);

		/* Update the cursor position. */
		fb_console_y = fb_console_rows - 1;
	}

	fb_console_toggle_cursor();
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
	fb_console_fillrect(0, 0, fb_console_mode.width, FONT_HEIGHT, FONT_BG);
	fb_console_toggle_cursor();
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

/** Draw a PPM image on the framebuffer.
 * @param ppm		Buffer containing PPM image.
 * @param x		X position of image.
 * @param y		Y position of image. */
static void ppm_draw(unsigned char *ppm, uint16_t x, uint16_t y) {
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
			fb_console_putpixel(x + j, y + i, colour);
		}
	}
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

	/* Register the console. */
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

		/* Clear the console to black. */
		fb_console_fillrect(0, 0, info.width, info.height, 0x0);

		/* Draw copyright text. */
		ppm_size(copyright_ppm, &width, &height);
		ppm_draw(copyright_ppm, (fb_console_mode.width / 2) - (width / 2),
		         fb_console_mode.height - height - 5);

		/* Get logo dimensions. */
		ppm_size(logo_ppm, &width, &height);

		/* Determine where to draw the progress bar. */
		splash_progress_x = (fb_console_mode.width / 2) - (SPLASH_PROGRESS_WIDTH / 2);
		splash_progress_y = (fb_console_mode.height / 2) + (height / 2) + 20;

		/* Draw logo. */
		ppm_draw(logo_ppm, (fb_console_mode.width / 2) - (width / 2),
		         (fb_console_mode.height / 2) - (height / 2) - 10);

		/* Draw initial progress bar. */
		console_update_boot_progress(0);
	} else {
		/* Clear the console to the font background colour. */
		fb_console_fillrect(0, 0, info.width, info.height, FONT_BG);
	}
}

/** Update the progress on the boot splash.
 * @param percent	Boot progress percentage. */
void console_update_boot_progress(int percent) {
	if(splash_enabled && !fb_console_acquired) {
		fb_console_fillrect(splash_progress_x, splash_progress_y,
		                    SPLASH_PROGRESS_WIDTH, SPLASH_PROGRESS_HEIGHT,
		                    SPLASH_PROGRESS_BG);
		fb_console_fillrect(splash_progress_x, splash_progress_y,
		                    (SPLASH_PROGRESS_WIDTH * percent) / 100,
		                    SPLASH_PROGRESS_HEIGHT, SPLASH_PROGRESS_FG);
	}
}
