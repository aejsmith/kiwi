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
 * @brief		VBE video setup code.
 */

#include <lib/list.h>
#include <lib/string.h>

#include <pc/bios.h>
#include <pc/vbe.h>

#include <console.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>
#include <video.h>

/** Structure describing a VBE video mode. */
typedef struct vbe_mode {
	video_mode_t header;		/**< Video mode header structure. */
	uint16_t id;			/**< ID of the mode. */
} vbe_mode_t;

/** Preferred/fallback video modes. */
#define PREFERRED_MODE_WIDTH		1024
#define PREFERRED_MODE_HEIGHT		768
#define FALLBACK_MODE_WIDTH		800
#define FALLBACK_MODE_HEIGHT		600

/** Override for video mode from Multiboot command line. */
char *video_mode_override = NULL;

/** Detect available video modes.
 * @todo		Handle VBE not being supported. */
void video_init(void) {
	vbe_mode_info_t *minfo = (vbe_mode_info_t *)(BIOS_MEM_BASE + sizeof(vbe_info_t));
	vbe_info_t *info = (vbe_info_t *)(BIOS_MEM_BASE);
	uint16_t *location;
	bios_regs_t regs;
	vbe_mode_t *mode;
	size_t i;

	/* Try to get controller information. */
	strncpy(info->vbe_signature, "VBE2", 4);
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_CONTROLLER_INFO;
	regs.edi = BIOS_MEM_BASE;
	bios_interrupt(0x10, &regs);
	if((regs.eax & 0xFF) != 0x4F) {
		boot_error("VBE is not supported");
	} else if((regs.eax & 0xFF00) != 0) {
		boot_error("Could not obtain VBE information (0x%x)", regs.eax & 0xFFFF);
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
			boot_error("Could not obtain VBE mode information (0x%x)", regs.eax & 0xFFFF);
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
		mode = kmalloc(sizeof(vbe_mode_t));
		mode->id = location[i];
		mode->header.width = minfo->x_resolution;
		mode->header.height = minfo->y_resolution;
		mode->header.bpp = minfo->bits_per_pixel;
		mode->header.addr = minfo->phys_base_ptr;
		video_mode_add(&mode->header);
	}

	/* Try to find the mode to use. */
	if(!video_mode_override || !(default_video_mode = video_mode_find_string(video_mode_override))) {
		if(!(default_video_mode = video_mode_find(PREFERRED_MODE_WIDTH,
		                                          PREFERRED_MODE_HEIGHT,
		                                          0))) {
			if(!(default_video_mode = video_mode_find(FALLBACK_MODE_WIDTH,
			                                          FALLBACK_MODE_HEIGHT,
			                                          0))) {
				boot_error("Could not find a usable video mode");
			}
		}
	}
}

/** Set the video mode.
 * @param mode		Mode to set. */
void video_enable(video_mode_t *mode) {
	vbe_mode_t *vmode = (vbe_mode_t *)mode;
	bios_regs_t regs;

	/* Set the mode. Bit 14 in the mode ID indicates that we wish to use
	 * the linear framebuffer model. */
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_SET_MODE;
	regs.ebx = vmode->id | (1<<14);
	bios_interrupt(0x10, &regs);

	dprintf("video: set video mode %dx%dx%d (framebuffer: 0x%" PRIpp ")\n",
	        mode->width, mode->height, mode->bpp, mode->addr);
}
