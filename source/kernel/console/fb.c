/*
 * Copyright (C) 2011-2012 Alex Smith
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
 * @brief		Framebuffer console.
 */

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/phys.h>

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

/** Dimensions and colours of the console font. */
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

/** Get the byte offset of a pixel. */
#define OFFSET(x, y)		(((y * fb_info.width) + x) * fb_info.bytes_per_pixel)

/** Framebuffer information. */
static fb_info_t fb_info;
static char *fb_mapping = NULL;
static char *fb_backbuffer = NULL;

/** Framebuffer console information. */
static unsigned char *fb_console_glyphs = NULL;
static uint16_t fb_console_cols;
static uint16_t fb_console_rows;
static uint16_t fb_console_x = 0;
static uint16_t fb_console_y = 0;
static bool fb_console_acquired = false;
static bool fb_was_acquired = false;

/** Boot splash information. */
static bool splash_enabled = false;
static uint16_t splash_progress_x;
static uint16_t splash_progress_y;

/** Put a pixel on the framebuffer.
 * @param x		X position.
 * @param y		Y position.
 * @param rgb		RGB colour to draw. */
static void fb_putpixel(uint16_t x, uint16_t y, uint32_t rgb) {
	uint32_t value = (RED(rgb, fb_info.red_size) << fb_info.red_position)
		| (GREEN(rgb, fb_info.green_size) << fb_info.green_position)
		| (BLUE(rgb, fb_info.blue_size) << fb_info.blue_position);
	size_t offset = OFFSET(x, y);
	void *fb_dest = fb_mapping + offset;
	void *bb_dest = fb_backbuffer + offset;

	switch(fb_info.bytes_per_pixel) {
	case 2:
		*(uint16_t *)fb_dest = (uint16_t)value;
		*(uint16_t *)bb_dest = (uint16_t)value;
		break;
	case 3:
		((uint8_t *)fb_dest)[0] = value & 0xff;
		((uint8_t *)fb_dest)[1] = (value >> 8) & 0xff;
		((uint8_t *)fb_dest)[2] = (value >> 16) & 0xff;
		((uint8_t *)bb_dest)[0] = value & 0xff;
		((uint8_t *)bb_dest)[1] = (value >> 8) & 0xff;
		((uint8_t *)bb_dest)[2] = (value >> 16) & 0xff;
		break;
	case 4:
		*(uint32_t *)fb_dest = value;
		*(uint32_t *)bb_dest = value;
		break;
	}
}

/** Draw a rectangle in a solid colour.
 * @param x		X position of rectangle.
 * @param y		Y position of rectangle.
 * @param width		Width of rectangle.
 * @param height	Height of rectangle.
 * @param rgb		Colour to draw in. */
static void fb_fillrect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t rgb) {
	uint16_t i, j;

	if(x == 0 && width == fb_info.width && (rgb == 0 || rgb == 0xffffff)) {
		/* Fast path where we can fill a block quickly. */
		memset(fb_mapping + OFFSET(0, y), (uint8_t)rgb, width * height * fb_info.bytes_per_pixel);
		memset(fb_backbuffer + OFFSET(0, y), (uint8_t)rgb, width * height * fb_info.bytes_per_pixel);
	} else {
		for(i = 0; i < height; i++) {
			for(j = 0; j < width; j++)
				fb_putpixel(x + j, y + i, rgb);
		}
	}
}

/** Copy part of the framebuffer to another location.
 * @param dest_y	Y position of destination.
 * @param src_y		Y position of source area.
 * @param height	Height of area to copy. */
static void fb_copyrect(uint16_t dest_y, uint16_t src_y, uint16_t height) {
	size_t dest_offset = OFFSET(0, dest_y);
	size_t src_offset = OFFSET(0, src_y);

	/* Copy everything on the backbuffer. */
	memmove(fb_backbuffer + dest_offset, fb_backbuffer + src_offset,
		fb_info.width * height * fb_info.bytes_per_pixel);

	/* Copy the updated backbuffer onto the framebuffer. */
	memcpy(fb_mapping + dest_offset, fb_backbuffer + dest_offset,
		fb_info.width * height * fb_info.bytes_per_pixel);
}

/** Draw the glyph at the specified position the console.
 * @param x		X position (characters).
 * @param y		Y position (characters).
 * @param fg		Foreground colour.
 * @param bg		Background colour. */
static void fb_console_draw_glyph(uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
	unsigned char ch;
	uint16_t i, j;

	/* Get the glyph. */
	ch = fb_console_glyphs[(y * fb_console_cols) + x];

	/* Convert to a pixel position. */
	x *= FONT_WIDTH;
	y *= FONT_HEIGHT;

	/* Draw the glyph. */
	for(i = 0; i < FONT_HEIGHT; i++) {
		for(j = 0; j < FONT_WIDTH; j++) {
			if(console_font[(ch * FONT_HEIGHT) + i] & (1<<(7-j))) {
				fb_putpixel(x + j, y + i, fg);
			} else {
				fb_putpixel(x + j, y + i, bg);
			}
		}
	}
}

/** Enable the cursor. */
static void fb_console_enable_cursor(void) {
	/* Draw in inverted colours. */
	fb_console_draw_glyph(fb_console_x, fb_console_y, FONT_BG, FONT_FG);
}

/** Disable the cursor. */
static void fb_console_disable_cursor(void) {
	/* Draw back in the correct colours. */
	fb_console_draw_glyph(fb_console_x, fb_console_y, FONT_FG, FONT_BG);
}

/** Write a character to the framebuffer console.
 * @param ch		Character to write. */
static void fb_console_putc(char ch) {
	if(fb_console_acquired) {
		return;
	}

	fb_console_disable_cursor();

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(fb_console_x) {
			fb_console_x--;
		} else if(fb_console_y) {
			fb_console_x = fb_console_cols - 1;
			fb_console_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		fb_console_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was there (will
		 * be handled below). */
		fb_console_x = fb_console_cols;
		break;
	case '\t':
		fb_console_x += 8 - (fb_console_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ')
			break;

		fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x] = ch;
		fb_console_draw_glyph(fb_console_x, fb_console_y, FONT_FG, FONT_BG);
		fb_console_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(fb_console_x >= fb_console_cols) {
		fb_console_x = 0;
		if(++fb_console_y < fb_console_rows)
			fb_fillrect(0, FONT_HEIGHT * fb_console_y, fb_info.width, FONT_HEIGHT, FONT_BG);
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(fb_console_y >= fb_console_rows) {
		/* Move everything up and fill the last row with blanks. */
		memmove(fb_console_glyphs, fb_console_glyphs + fb_console_cols,
			(fb_console_rows - 1) * fb_console_cols);
		memset(fb_console_glyphs + ((fb_console_rows - 1) * fb_console_cols),
			' ', fb_console_cols);
		fb_copyrect(0, FONT_HEIGHT, (fb_console_rows - 1) * FONT_HEIGHT);
		fb_fillrect(0, FONT_HEIGHT * (fb_console_rows - 1), fb_info.width,
			FONT_HEIGHT, FONT_BG);

		/* Update the cursor position. */
		fb_console_y = fb_console_rows - 1;
	}

	fb_console_enable_cursor();
}

/** Kernel console output operations structure. */
static console_out_ops_t fb_console_out_ops = {
	.putc = fb_console_putc,
};

/** Reset the framebuffer console. */
static void fb_console_reset(void) {
	/* Reset the cursor position and clear the first row of the console. */
	fb_console_x = fb_console_y = 0;
	fb_fillrect(0, 0, fb_info.width, FONT_HEIGHT, FONT_BG);
	memset(fb_console_glyphs, ' ', fb_console_cols * fb_console_rows);
	fb_console_enable_cursor();
}

/** Enable the framebuffer console upon KDB entry/fatal().
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void fb_console_enable(void *arg1, void *arg2, void *arg3) {
	if(screen_console_ops == &fb_console_out_ops) {
		fb_was_acquired = fb_console_acquired;
		if(fb_was_acquired) {
			fb_console_acquired = false;
			fb_console_reset();
		}
	}
}

/** Disable the framebuffer console upon KDB exit.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void fb_console_disable(void *arg1, void *arg2, void *arg3) {
	if(screen_console_ops == &fb_console_out_ops)
		fb_console_acquired = fb_was_acquired;
}

/** Reconfigure the framebuffer console.
 * @param info		Information structure for new framebuffer.
 * @param mmflag	Allocation behaviour flags (MM_WAIT or MM_BOOT). */
static void fb_console_configure(const fb_info_t *info, int mmflag) {
	size_t size;

	/* Temporarily disable the framebuffer console to ensure nothing prints
	 * to it while we're making the change. */
	if(screen_console_ops == &fb_console_out_ops) {
		/* Framebuffer console is already in use. Temporarily disable
		 * it to ensure nothing prints to it while we're making
		 * changes. */
		screen_console_ops = NULL;

		/* Free old mappings. */
		size = fb_info.width * fb_info.height * fb_info.bytes_per_pixel;
		phys_unmap(fb_mapping, size, true);
		kmem_free(fb_backbuffer, ROUND_UP(size, PAGE_SIZE));
		kfree(fb_console_glyphs);
	} else {
		/* First time the framebuffer console has been enabled.
		 * Register callbacks to reset the framebuffer console upon
		 * fatal() and KDB entry. */
		notifier_register(&fatal_notifier, fb_console_enable, NULL);
		notifier_register(&kdb_entry_notifier, fb_console_enable, NULL);
		notifier_register(&kdb_exit_notifier, fb_console_disable, NULL);
	}

	memcpy(&fb_info, info, sizeof(fb_info));

	/* Map in the framebuffer, clear it and allocate a backbuffer. */
	size = fb_info.width * fb_info.height * fb_info.bytes_per_pixel;
	fb_mapping = phys_map(fb_info.addr, size, mmflag);
	memset(fb_mapping, 0, size);
	fb_backbuffer = kmem_alloc(ROUND_UP(size, PAGE_SIZE), mmflag | MM_ZERO);

	/* Configure the console and create a backbuffer for it, initially
	 * filled with spaces. */
	fb_console_x = fb_console_y = 0;
	fb_console_cols = fb_info.width / FONT_WIDTH;
	fb_console_rows = fb_info.height / FONT_HEIGHT;
	fb_console_glyphs = kmalloc(fb_console_cols * fb_console_rows, mmflag);
	memset(fb_console_glyphs, ' ', fb_console_cols * fb_console_rows);

	if(!fb_console_acquired) {
		/* Clear to the font background colour. */
		fb_fillrect(0, 0, fb_info.width, fb_info.height, FONT_BG);
		fb_console_enable_cursor();
	}

	screen_console_ops = &fb_console_out_ops;
}

/** Control the framebuffer console.
 * @param op		Operation to perform.
 * @param info		For FB_CONSOLE_INFO and FB_CONSOLE_CONFIGURE, FB info
 *			structure to use. */
void fb_console_control(unsigned op, fb_info_t *info) {
	switch(op) {
	case FB_CONSOLE_INFO:
		/* Get information on the current mode. */
		memcpy(info, &fb_info, sizeof(*info));
		break;
	case FB_CONSOLE_CONFIGURE:
		/* Reconfigure to use a new framebuffer. */
		fb_console_configure(info, MM_WAIT);
		break;
	case FB_CONSOLE_ACQUIRE:
		/* Acquire the framebuffer for exclusive use. */
		if(fb_console_acquired && !splash_enabled)
			fatal("Framebuffer console already acquired");

		/* Prevent output to the console. */
		fb_console_acquired = true;
		splash_enabled = false;
		break;
	case FB_CONSOLE_RELEASE:
		/* Release the framebuffer. */
		if(!fb_console_acquired)
			fatal("Framebuffer console not acquired");

		fb_console_acquired = false;
		fb_console_reset();
		break;
	}
}

/* Skip over whitespace and comments in a PPM file.
 * @param buf		Pointer to data buffer.
 * @return		Address of next non-whitespace byte. */
static unsigned char *ppm_skip(unsigned char *buf) {
	while(true) {
		while(isspace(*buf))
			buf++;

		if(*buf == '#') {
			while(*buf != '\n' && *buf != '\r')
				buf++;
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

	if((ppm[0] != 'P') || (ppm[1] != '6'))
		return;

	ppm += 2;

	ppm = ppm_skip(ppm);
	width = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm = ppm_skip(ppm);
	height = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm = ppm_skip(ppm);
	max_colour = strtoul((const char *)ppm, (char **)&ppm, 10);
	ppm++;

	if(!max_colour || max_colour > 255)
		return;

	coef = 255 / max_colour;
	if((coef * max_colour) > 255)
		coef -= 1;

	/* Draw the image. */
	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			colour = 0;
			colour |= (*(ppm++) * coef) << 16;
			colour |= (*(ppm++) * coef) << 8;
			colour |= *(ppm++) * coef;
			fb_putpixel(x + j, y + i, colour);
		}
	}
}

/** Initialize the framebuffer console. */
__init_text void fb_console_init(void) {
	uint16_t width, height;
	kboot_tag_lfb_t *lfb;
	fb_info_t info;

	/* Look up the framebuffer boot tag. */
	lfb = kboot_tag_iterate(KBOOT_TAG_LFB, NULL);
	if(!lfb)
		fatal("Expected LFB but no boot tag");

	/* Copy the information from it. Currently guessing the size/position
	 * values, see KBoot issue #8. */
	info.width = lfb->width;
	info.height = lfb->height;
	info.depth = lfb->depth;
	info.bytes_per_pixel = ROUND_UP(lfb->depth, 8) / 8;
	info.addr = lfb->addr;
	switch(info.depth) {
	case 15:
		/* RGB 5:5:5. */
		info.red_position = 10;
		info.red_size = 5;
		info.green_position = 5;
		info.green_size = 5;
		info.blue_position = 0;
		info.blue_size = 5;
		break;
	case 16:
		/* RGB 5:6:5. */
		info.red_position = 11;
		info.red_size = 5;
		info.green_position = 5;
		info.green_size = 6;
		info.blue_position = 0;
		info.blue_size = 5;
		break;
	case 24:
		/* RGB 8:8:8. */
	case 32:
		/* ARGB 0:8:8:8. */
		info.red_position = 16;
		info.red_size = 8;
		info.green_position = 8;
		info.green_size = 8;
		info.blue_position = 0;
		info.blue_size = 8;
		break;
	default:
		fatal("Unsupported framebuffer depth");
	}

	kboot_tag_release(lfb);

	/* If the splash is enabled, acquire the console so output is ignored. */
	if(!kboot_boolean_option("splash_disabled")) {
		splash_enabled = true;
		fb_console_acquired = true;
	}

	/* Configure the framebuffer. */
	fb_console_configure(&info, MM_BOOT);

	/* Draw the splash screen if enabled. */
	if(splash_enabled) {
		/* Draw copyright text. */
		ppm_size(copyright_ppm, &width, &height);
		ppm_draw(copyright_ppm, (fb_info.width / 2) - (width / 2),
			fb_info.height - height - 5);

		/* Get logo dimensions. */
		ppm_size(logo_ppm, &width, &height);

		/* Determine where to draw the progress bar. */
		splash_progress_x = (fb_info.width / 2) - (SPLASH_PROGRESS_WIDTH / 2);
		splash_progress_y = (fb_info.height / 2) + (height / 2) + 20;

		/* Draw logo. */
		ppm_draw(logo_ppm, (fb_info.width / 2) - (width / 2),
			(fb_info.height / 2) - (height / 2) - 10);

		/* Draw initial progress bar. */
		update_boot_progress(0);
	}
}

/** Update the progress on the boot splash.
 * @param percent	Boot progress percentage. */
void update_boot_progress(int percent) {
	if(splash_enabled) {
		fb_fillrect(splash_progress_x, splash_progress_y,
			SPLASH_PROGRESS_WIDTH, SPLASH_PROGRESS_HEIGHT,
			SPLASH_PROGRESS_BG);
		fb_fillrect(splash_progress_x, splash_progress_y,
			(SPLASH_PROGRESS_WIDTH * percent) / 100,
			SPLASH_PROGRESS_HEIGHT, SPLASH_PROGRESS_FG);
	}
}
