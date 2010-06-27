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
 * @brief		VBE video setup code.
 */

#include <boot/console.h>
#include <boot/memory.h>
#include <boot/menu.h>

#include <platform/bios.h>
#include <platform/boot.h>
#include <platform/vbe.h>

#include <lib/list.h>
#include <lib/string.h>

#include <fatal.h>
#include <kargs.h>

/** Structure describing a video mode. */
typedef struct video_mode {
	list_t header;			/**< Link to mode list. */
	uint16_t id;			/**< ID of the mode. */
	int width;			/**< Mode width. */
	int height;			/**< Mode height. */
	int bpp;			/**< Bits per pixel. */
	phys_ptr_t addr;		/**< Physical address of the framebuffer. */
} video_mode_t;

/** Preferred/fallback video modes. */
#define PREFERRED_MODE_WIDTH		1024
#define PREFERRED_MODE_HEIGHT		768
#define FALLBACK_MODE_WIDTH		800
#define FALLBACK_MODE_HEIGHT		600

/** List of video modes. */
static LIST_DECLARE(video_modes);

/** Detected video mode. */
static video_mode_t *detected_video_mode = NULL;

/** Menu choice for the video mode. */
//static menu_item_t *video_mode_choice = NULL;

/** Override for video mode from Multiboot command line. */
char *video_mode_override = NULL;

/** Search for a video mode.
 * @param width		Width to search for.
 * @param height	Height to search for.
 * @param depth		Depth to search for or 0 to use the highest depth found.
 * @return		Pointer to mode structure, or NULL if not found. */
static video_mode_t *video_mode_find(int width, int height, int depth) {
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

/** Get the mode specified by the override string.
 * @return		Mode if found, NULL if not. */
static video_mode_t *get_override_mode(void) {
	int width = 0, height = 0, depth = 0;
	char *tok;

	if(video_mode_override) {
		if((tok = strsep(&video_mode_override, "x"))) {
			width = strtol(tok, NULL, 0);
		}
		if((tok = strsep(&video_mode_override, "x"))) {
			height = strtol(tok, NULL, 0);
		}
		if((tok = strsep(&video_mode_override, "x"))) {
			depth = strtol(tok, NULL, 0);
		}

		if(width && height) {
			return video_mode_find(width, height, depth);
		}
	}

	return NULL;
}

#if 0
/** Add PC-specific options to the menu.
 * @param menu		Main menu.
 * @param options	Options menu. */
void platform_add_menu_options(menu_t *menu, menu_t *options) {
	video_mode_t *mode;
	char *str;

	/* Add a list to choose the video mode. */
	video_mode_choice = menu_add_choice(menu, "Video Mode");
	LIST_FOREACH(&video_modes, iter) {
		mode = list_entry(iter, video_mode_t, header);

		/* Create a string for the mode. */
		str = kmalloc(16);
		sprintf(str, "%dx%dx%d", mode->width, mode->height, mode->bpp);

		menu_item_add_choice(video_mode_choice, str, mode, mode == detected_video_mode);
	}
}
#endif

/** Detect available video modes.
 * @todo		Handle VBE not being supported. */
void platform_video_init(void) {
	vbe_mode_info_t *minfo = (vbe_mode_info_t *)(BIOS_MEM_BASE + sizeof(vbe_info_t));
	vbe_info_t *info = (vbe_info_t *)(BIOS_MEM_BASE);
	video_mode_t *mode;
	uint16_t *location;
	bios_regs_t regs;
	size_t i;

	/* Try to get controller information. */
	strncpy(info->vbe_signature, "VBE2", 4);
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_CONTROLLER_INFO;
	regs.edi = BIOS_MEM_BASE;
	bios_interrupt(0x10, &regs);
	if((regs.eax & 0xFF) != 0x4F) {
		fatal("VBE is not supported");
	} else if((regs.eax & 0xFF00) != 0) {
		fatal("Could not obtain VBE information (0x%x)", regs.eax & 0xFFFF);
	}

	dprintf("vbe: vbe presence was detected:\n");
	dprintf(" signature:    %s\n", info->vbe_signature);
	dprintf(" version:      %u.%u\n", info->vbe_version_major, info->vbe_version_minor);
	dprintf(" capabilities: 0x%" PRIx32 "\n", info->capabilities);
	dprintf(" mode pointer: 0x%" PRIx32 "\n", info->video_mode_ptr);
	dprintf(" total memory: %" PRIu16 "KB\n", info->total_memory * 64);
	if(info->vbe_version_major >= 2) {
		dprintf(" OEM revision: 0x%" PRIx16 "\n", info->oem_software_rev);
	}

	/* Iterate through the modes. 0xFFFF indicates the end of the list. */
	location = (uint16_t *)SEGOFF2LIN(info->video_mode_ptr);
	for(i = 0; location[i] != 0xFFFF; i++) {
		bios_regs_init(&regs);
		regs.eax = VBE_FUNCTION_MODE_INFO;
		regs.ecx = location[i];
		regs.edi = BIOS_MEM_BASE + sizeof(vbe_info_t);
		bios_interrupt(0x10, &regs);
		if((regs.eax & 0xFF00) != 0) {
			fatal("Could not obtain VBE mode information (0x%x)", regs.eax & 0xFFFF);
		}

		/* Check if the mode is suitable. */
		if(minfo->memory_model != 4 && minfo->memory_model != 6) {
			/* Not packed-pixel or direct colour. */
			continue;
		} else if(minfo->phys_base_ptr == 0) {
			/* Lulwhut? */
			continue;
		} else if((minfo->mode_attributes & (1<<0)) == 0) {
			/* Not supported. */
			continue;
		} else if((minfo->mode_attributes & (1<<3)) == 0) {
			/* Not colour. */
			continue;
		} else if((minfo->mode_attributes & (1<<4)) == 0) {
			/* Not a graphics mode. */
			continue;
		} else if((minfo->mode_attributes & (1<<7)) == 0) {
			/* Not usable in linear mode. */
			continue;
		} else if(minfo->bits_per_pixel != 8 && minfo->bits_per_pixel != 16 &&
		          minfo->bits_per_pixel != 24 && minfo->bits_per_pixel != 32) {
			continue;
		}

		/* Add the mode to the list. */
		mode = kmalloc(sizeof(video_mode_t));
		list_init(&mode->header);
		mode->id = location[i];
		mode->width = minfo->x_resolution;
		mode->height = minfo->y_resolution;
		mode->bpp = minfo->bits_per_pixel;
		mode->addr = minfo->phys_base_ptr;
		list_append(&video_modes, &mode->header);
	}

	if(list_empty(&video_modes)) {
		fatal("No usable video modes detected");
	}

	/* Try to find the mode to use. */
	if(!(detected_video_mode = get_override_mode())) {
		if(!(detected_video_mode = video_mode_find(PREFERRED_MODE_WIDTH,
		                                           PREFERRED_MODE_HEIGHT,
		                                           0))) {
			if(!(detected_video_mode = video_mode_find(FALLBACK_MODE_WIDTH,
			                                           FALLBACK_MODE_HEIGHT,
			                                           0))) {
				fatal("Could not find video mode to use");
			}
		}
	}
}

/** Set the video mode. */
void platform_video_enable(void) {
	video_mode_t *mode;
	bios_regs_t regs;

	/* Get the video mode to set. If the menu has been displayed, use the
	 * mode that was selected there, else use the best mode. */
#if 0
	if(video_mode_choice) {
		mode = (video_mode_t *)video_mode_choice->value;
	} else {
		mode = detected_video_mode;
	}
#endif
	mode = detected_video_mode;

	/* Set the mode. Bit 14 in the mode ID indicates that we wish to use
	 * the linear framebuffer model. */
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_SET_MODE;
	regs.ebx = mode->id | (1<<14);
	bios_interrupt(0x10, &regs);

	dprintf("video: set video mode %dx%dx%d (framebuffer: 0x%" PRIpp ")\n",
	        mode->width, mode->height, mode->bpp, mode->addr);

	/* Write mode information to the kernel arguments. */
	kernel_args->fb_width = mode->width;
	kernel_args->fb_height = mode->height;
	kernel_args->fb_depth = mode->bpp;
	kernel_args->fb_addr = mode->addr;
}
