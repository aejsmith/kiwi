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

#include <boot/memory.h>
#include <boot/video.h>

#include <lib/string.h>

/** List of video modes. */
static LIST_DECLARE(video_modes);

/** Default video mode. */
video_mode_t *default_video_mode = NULL;

/** Search for a video mode.
 * @param width		Width to search for.
 * @param height	Height to search for.
 * @param depth		Depth to search for or 0 to use the highest depth found.
 * @return		Pointer to mode structure, or NULL if not found. */
video_mode_t *video_mode_find(int width, int height, int depth) {
	video_mode_t *mode, *ret = NULL;

	LIST_FOREACH(&video_modes, iter) {
		mode = list_entry(iter, video_mode_t, header);
		if(mode->width == width && mode->height == height) {
			if(depth) {
				if(mode->bpp == depth) {
					return mode;
				}
			} else if(!ret || mode->bpp > ret->bpp) {
				ret = mode;
			}
		}
	}

	return ret;
}

/** Search for a video mode.
 * @param mode		String describing mode to search for.
 * @return		Pointer to video mode if found, NULL if not. */
video_mode_t *video_mode_find_string(const char *mode) {
	int width = 0, height = 0, depth = 0;
	char *dup, *orig, *tok;

	dup = orig = kstrdup(mode);
	if((tok = strsep(&dup, "x"))) {
		width = strtol(tok, NULL, 0);
	}
	if((tok = strsep(&dup, "x"))) {
		height = strtol(tok, NULL, 0);
	}
	if((tok = strsep(&dup, "x"))) {
		depth = strtol(tok, NULL, 0);
	}
	kfree(orig);

	return (width && height) ? video_mode_find(width, height, depth) : NULL;
}

/** Add a video mode.
 * @param mode		Mode to add. */
void video_mode_add(video_mode_t *mode) {
	list_init(&mode->header);
	mode->name = kmalloc(16);
	sprintf(mode->name, "%dx%dx%d", mode->width, mode->height, mode->bpp);
	list_append(&video_modes, &mode->header);
}

/** Generate a video mode chooser.
 * @param label		Label to give the chooser.
 * @param value		Value to store choice in.
 * @return		Pointer to chooser. */
ui_entry_t *video_mode_chooser(const char *label, value_t *value) {
	ui_entry_t *chooser = ui_chooser_create(label, value);
	void *current = value->pointer;
	video_mode_t *mode;

	LIST_FOREACH(&video_modes, iter) {
		mode = list_entry(iter, video_mode_t, header);
		ui_chooser_insert(chooser, mode->name, mode, mode == current);
	}

	return chooser;
}
