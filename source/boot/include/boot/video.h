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
