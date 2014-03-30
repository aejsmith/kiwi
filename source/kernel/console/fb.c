/*
 * Copyright (C) 2011-2013 Alex Smith
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

#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

#if CONFIG_DEBUG
KBOOT_BOOLEAN_OPTION("splash_disabled", "Disable splash screen", true);
#else
KBOOT_BOOLEAN_OPTION("splash_disabled", "Disable splash screen", false);
#endif

extern unsigned char console_font[];
extern unsigned char logo_ppm[];

/** Dimensions and colours of the console font. */
#define FONT_WIDTH		7
#define FONT_HEIGHT		14
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

/** Lock for the framebuffer console. */
static SPINLOCK_DEFINE(fb_lock);

/** Framebuffer information. */
static fb_info_t fb_info;
static uint8_t *fb_mapping = NULL;
static uint8_t *fb_backbuffer = NULL;

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

/**
 * Framebuffer drawing functions.
 */

/** Put a pixel on the framebuffer.
 * @param x		X position.
 * @param y		Y position.
 * @param rgb		RGB colour to draw. */
static void fb_putpixel(uint16_t x, uint16_t y, uint32_t rgb) {
	uint32_t value = (RED(rgb, fb_info.red_size) << fb_info.red_position)
		| (GREEN(rgb, fb_info.green_size) << fb_info.green_position)
		| (BLUE(rgb, fb_info.blue_size) << fb_info.blue_position);
	size_t offset = OFFSET(x, y);
	void *dest = fb_backbuffer + offset;

	switch(fb_info.bytes_per_pixel) {
	case 2:
		*(uint16_t *)dest = (uint16_t)value;
		break;
	case 3:
		((uint8_t *)dest)[0] = value & 0xff;
		((uint8_t *)dest)[1] = (value >> 8) & 0xff;
		((uint8_t *)dest)[2] = (value >> 16) & 0xff;
		break;
	case 4:
		*(uint32_t *)dest = value;
		break;
	}

	if(fb_backbuffer != fb_mapping) {
		dest = fb_mapping + offset;

		switch(fb_info.bytes_per_pixel) {
		case 2:
			*(uint16_t *)dest = (uint16_t)value;
			break;
		case 3:
			((uint8_t *)dest)[0] = value & 0xff;
			((uint8_t *)dest)[1] = (value >> 8) & 0xff;
			((uint8_t *)dest)[2] = (value >> 16) & 0xff;
			break;
		case 4:
			*(uint32_t *)dest = value;
			break;
		}
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
		memset(fb_backbuffer + OFFSET(0, y), (uint8_t)rgb,
			width * height * fb_info.bytes_per_pixel);
		if(fb_backbuffer != fb_mapping) {
			memset(fb_mapping + OFFSET(0, y), (uint8_t)rgb,
				width * height * fb_info.bytes_per_pixel);
		}
	} else {
		for(i = 0; i < height; i++) {
			for(j = 0; j < width; j++)
				fb_putpixel(x + j, y + i, rgb);
		}
	}
}

/** Copy part of the framebuffer to another location.
 * @param dest_x	X position of destination.
 * @param dest_y	Y position of destination.
 * @param src_x		X position of source area.
 * @param src_y		Y position of source area.
 * @param width		Width of area to copy.
 * @param height	Height of area to copy. */
static void fb_copyrect(uint16_t dest_x, uint16_t dest_y, uint16_t src_x,
	uint16_t src_y, uint16_t width, uint16_t height)
{
	size_t dest_offset, src_offset;
	uint16_t i;

	if(dest_x == 0 && src_x == 0 && width == fb_info.width) {
		/* Fast path where we can copy everything in one go. */
		dest_offset = OFFSET(0, dest_y);
		src_offset = OFFSET(0, src_y);

		/* Copy everything on the backbuffer. */
		memmove(fb_backbuffer + dest_offset, fb_backbuffer + src_offset,
			fb_info.width * height * fb_info.bytes_per_pixel);

		if(fb_backbuffer != fb_mapping) {
			/* Copy the updated backbuffer onto the framebuffer. */
			memcpy(fb_mapping + dest_offset, fb_backbuffer + dest_offset,
				fb_info.width * height * fb_info.bytes_per_pixel);
		}
	} else {
		/* Copy line by line. */
		for(i = 0; i < height; i++) {
			dest_offset = OFFSET(dest_x, dest_y + i);
			src_offset = OFFSET(src_x, src_y + i);

			/* Copy everything on the backbuffer. */
			memmove(fb_backbuffer + dest_offset, fb_backbuffer + src_offset,
				width * fb_info.bytes_per_pixel);

			if(fb_backbuffer != fb_mapping) {
				/* Copy the updated backbuffer onto the framebuffer. */
				memcpy(fb_mapping + dest_offset, fb_backbuffer + dest_offset,
					width * fb_info.bytes_per_pixel);
			}
		}
	}
}

/**
 * Framebuffer console functions.
 */

/** Draw a glyph at the specified position the console.
 * @param ch		Character to draw.
 * @param x		X position (characters).
 * @param y		Y position (characters).
 * @param fg		Foreground colour.
 * @param bg		Background colour. */
static void fb_console_draw_glyph(char ch, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
	uint16_t i, j;

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
	if(fb_console_glyphs) {
		fb_console_draw_glyph(
			fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x],
			fb_console_x, fb_console_y, FONT_BG, FONT_FG);
	}
}

/** Disable the cursor. */
static void fb_console_disable_cursor(void) {
	/* Draw back in the correct colours. */
	if(fb_console_glyphs) {
		fb_console_draw_glyph(
			fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x],
			fb_console_x, fb_console_y, FONT_FG, FONT_BG);
	}
}

/** Write a character to the framebuffer console.
 * @param ch		Character to write. */
static void fb_console_putc(char ch) {
	if(fb_console_acquired)
		return;

	spinlock_lock(&fb_lock);

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

		if(fb_console_glyphs)
			fb_console_glyphs[(fb_console_y * fb_console_cols) + fb_console_x] = ch;

		fb_console_draw_glyph(ch, fb_console_x, fb_console_y, FONT_FG, FONT_BG);
		fb_console_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(fb_console_x >= fb_console_cols) {
		fb_console_x = 0;
		if(++fb_console_y < fb_console_rows) {
			fb_fillrect(0, FONT_HEIGHT * fb_console_y, fb_info.width,
				FONT_HEIGHT, FONT_BG);
		}
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(fb_console_y >= fb_console_rows) {
		/* Move everything up and fill the last row with blanks. */
		if(fb_console_glyphs) {
			memmove(fb_console_glyphs, fb_console_glyphs + fb_console_cols,
				(fb_console_rows - 1) * fb_console_cols);
			memset(fb_console_glyphs + ((fb_console_rows - 1) * fb_console_cols),
				' ', fb_console_cols);
		}
		fb_copyrect(0, 0, 0, FONT_HEIGHT, fb_info.width,
			(fb_console_rows - 1) * FONT_HEIGHT);
		fb_fillrect(0, FONT_HEIGHT * (fb_console_rows - 1), fb_info.width,
			FONT_HEIGHT, FONT_BG);

		/* Update the cursor position. */
		fb_console_y = fb_console_rows - 1;
	}

	fb_console_enable_cursor();
	spinlock_unlock(&fb_lock);
}

/** Properly initialize the framebuffer console.
 * @param video		KBoot video tag. */
static void fb_console_init(kboot_tag_video_t *video) {
	fb_console_configure(&fb_info, MM_BOOT);
}

/** Kernel console output operations structure. */
static console_out_ops_t fb_console_out_ops = {
	.init = fb_console_init,
	.putc = fb_console_putc,
};

/** Reset the framebuffer console. */
static void fb_console_reset(void) {
	/* Reset the cursor position and clear the console. */
	fb_console_x = fb_console_y = 0;
	fb_fillrect(0, 0, fb_info.width, fb_info.height, FONT_BG);
	memset(fb_console_glyphs, ' ', fb_console_cols * fb_console_rows);
	fb_console_enable_cursor();
}

/** Enable the framebuffer console upon KDB entry/fatal().
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void fb_console_enable(void *arg1, void *arg2, void *arg3) {
	if(main_console.out == &fb_console_out_ops) {
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
	if(main_console.out == &fb_console_out_ops)
		fb_console_acquired = fb_was_acquired;
}

/**
 * Public functions.
 */

/** Get framebuffer console information.
 * @param info		Where to store console information. */
void fb_console_info(fb_info_t *info) {
	spinlock_lock(&fb_lock);
	memcpy(info, &fb_info, sizeof(*info));
	spinlock_unlock(&fb_lock);
}

/** Reconfigure the framebuffer console.
 * @param info		Information for the new framebuffer.
 * @param mmflag	Allocation behaviour flags.
 * @return		Status code describing the result of the operation. */
status_t fb_console_configure(const fb_info_t *info, unsigned mmflag) {
	size_t size;
	uint16_t cols, rows;
	uint8_t *mapping;
	uint8_t *backbuffer;
	unsigned char *glyphs;
	bool was_boot, have_prev;

	/* Map in the framebuffer and allocate a backbuffer. */
	size = info->width * info->height * info->bytes_per_pixel;
	size = ROUND_UP(size, PAGE_SIZE);
	mapping = phys_map(fb_info.addr, size, mmflag);
	if(!mapping)
		return STATUS_NO_MEMORY;

	backbuffer = kmem_alloc(size, mmflag);
	if(!backbuffer) {
		phys_unmap(fb_mapping, size, true);
		return STATUS_NO_MEMORY;
	}

	cols = info->width / FONT_WIDTH;
	rows = info->height / FONT_HEIGHT;
	glyphs = kmalloc(cols * rows, mmflag);
	if(!glyphs) {
		kmem_free(backbuffer, size);
		phys_unmap(fb_mapping, size, true);
		return STATUS_NO_MEMORY;
	}

	spinlock_lock(&fb_lock);

	was_boot = fb_backbuffer == fb_mapping;
	have_prev = main_console.out == &fb_console_out_ops && !was_boot;
	size = fb_info.width * fb_info.height * fb_info.bytes_per_pixel;
	size = ROUND_UP(size, PAGE_SIZE);

	/* Swap out the old framebuffer for the new one. */
	memcpy(&fb_info, info, sizeof(fb_info));
	SWAP(fb_mapping, mapping);
	SWAP(fb_backbuffer, backbuffer);
	SWAP(fb_console_glyphs, glyphs);
	fb_console_cols = cols;
	fb_console_rows = rows;

	memset(fb_console_glyphs, ' ', fb_console_cols * fb_console_rows);

	/* If this was the KBoot framebuffer we should copy the current content
	 * of that to the backbuffer. */
	if(was_boot) {
		memcpy(fb_backbuffer, fb_mapping, size);
	} else if(!fb_console_acquired) {
		fb_console_x = fb_console_y = 0;

		/* Clear to the font background colour. */
		fb_fillrect(0, 0, fb_info.width, fb_info.height, FONT_BG);
		fb_console_enable_cursor();
	}

	main_console.out = &fb_console_out_ops;

	spinlock_unlock(&fb_lock);

	if(have_prev) {
		/* Free old mappings. */
		kfree(glyphs);
		kmem_free(backbuffer, size);
		phys_unmap(mapping, size, true);
	} else {
		/* First time the framebuffer console has been enabled.
		 * Register callbacks to reset the framebuffer console upon
		 * fatal() and KDB entry. */
		notifier_register(&fatal_notifier, fb_console_enable, NULL);
		notifier_register(&kdb_entry_notifier, fb_console_enable, NULL);
		notifier_register(&kdb_exit_notifier, fb_console_disable, NULL);
	}

	return STATUS_SUCCESS;
}

/**
 * Acquire the framebuffer for exclusive use.
 *
 * Acquires the framebuffer for exclusive use. This disables the splash screen
 * and prevents kernel output to the framebuffer. It can be used if KDB is
 * entered or a fatal error occurs.
 */
void fb_console_acquire(void) {
	spinlock_lock(&fb_lock);

	if(fb_console_acquired && !splash_enabled)
		fatal("Framebuffer console already acquired");

	fb_console_acquired = true;
	splash_enabled = false;

	spinlock_unlock(&fb_lock);
}

/**
 * Release the framebuffer.
 *
 * Releases the framebuffer after it has been acquired with fb_console_acquire()
 * and re-enables kernel output to it.
 */
void fb_console_release(void) {
	spinlock_lock(&fb_lock);

	if(!fb_console_acquired)
		fatal("Framebuffer console not acquired");

	fb_console_acquired = false;
	fb_console_reset();

	spinlock_unlock(&fb_lock);
}

/**
 * Splash screen functions.
 */

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

/**
 * Initialization functions.
 */

/** Initialize the framebuffer console.
 * @param video		KBoot video tag. */
__init_text void fb_console_early_init(kboot_tag_video_t *video) {
	uint16_t width, height;

	/* Copy the information from the video tag. */
	fb_info.width = video->lfb.width;
	fb_info.height = video->lfb.height;
	fb_info.depth = video->lfb.bpp;
	fb_info.bytes_per_pixel = ROUND_UP(video->lfb.bpp, 8) / 8;
	fb_info.addr = video->lfb.fb_phys;
	fb_info.red_position = video->lfb.red_pos;
	fb_info.red_size = video->lfb.red_size;
	fb_info.green_position = video->lfb.green_pos;
	fb_info.green_size = video->lfb.green_size;
	fb_info.blue_position = video->lfb.blue_pos;
	fb_info.blue_size = video->lfb.blue_size;

	/* Get the mapping created by KBoot. Can't create a backbuffer yet so
	 * set the backbuffer pointer to be the same - this will cause updates
	 * from the backbuffer to not be done. */
	fb_mapping = (uint8_t *)((ptr_t)video->lfb.fb_virt);
	fb_backbuffer = fb_mapping;

	/* Clear the framebuffer. */
	fb_fillrect(0, 0, fb_info.width, fb_info.height, FONT_BG);

	/* Configure the console. */
	fb_console_x = fb_console_y = 0;
	fb_console_cols = fb_info.width / FONT_WIDTH;
	fb_console_rows = fb_info.height / FONT_HEIGHT;

	main_console.out = &fb_console_out_ops;

	/* If the splash is enabled, acquire the console so output is ignored. */
	if(!kboot_boolean_option("splash_disabled")) {
		splash_enabled = true;
		fb_console_acquired = true;

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
