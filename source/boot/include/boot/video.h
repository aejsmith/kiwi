/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Bootloader video functions.
 */

#ifndef __BOOT_VIDEO_H
#define __BOOT_VIDEO_H

#include <boot/ui.h>

#include <lib/list.h>

/** Structure describing a video mode. */
typedef struct video_mode {
	list_t header;			/**< Link to video modes list. */
	char *name;			/**< Name of the mode. */

	/** To be filled in by platform code. */
	int width;			/**< Mode width. */
	int height;			/**< Mode height. */
	int bpp;			/**< Bits per pixel. */
	phys_ptr_t addr;		/**< Physical address of the framebuffer. */
} video_mode_t;

extern video_mode_t *default_video_mode;

extern video_mode_t *video_mode_find(int width, int height, int depth);
extern video_mode_t *video_mode_find_string(const char *mode);
extern void video_mode_add(video_mode_t *mode);
extern ui_entry_t *video_mode_chooser(const char *label, value_t *value);

extern void video_init(void);
extern void video_enable(video_mode_t *mode);

#endif /* __BOOT_VIDEO_H */
