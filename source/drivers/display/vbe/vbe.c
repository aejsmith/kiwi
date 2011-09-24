/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		VBE display driver.
 */

#include <pc/bios.h>

#include <drivers/display.h>

#include <mm/malloc.h>
#include <mm/phys.h>

#include <kdbg.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

#include "vbe_priv.h"

/** Display device structure. */
static device_t *vbe_display_device;

/** Set the display mode.
 * @param _dev		Device to set mode of.
 * @param mode		Mode structure for mode to set.
 * @return		Status code describing result of the operation. */
static status_t vbe_display_set_mode(display_device_t *_device, display_mode_t *mode) {
	bios_regs_t regs;

	if(mode) {
		/* Check if anything needs to be done. */
		bios_regs_init(&regs);
		regs.eax = VBE_FUNCTION_GET_MODE;
		bios_interrupt(0x10, &regs);
		if((regs.eax & 0xFF00) != 0) {
			kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
			return STATUS_DEVICE_ERROR;
		}
		if((regs.ebx & ~((1<<14) | (1<<15))) == mode->id) {
			return STATUS_SUCCESS;
		}

		kprintf(LOG_DEBUG, "vbe: switching to mode 0x%" PRIx32 " (mode: %p)\n", mode->id, mode);
	}

	/* Set bit 14 in the mode register to use linear framebuffer model. */
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_SET_MODE;
	regs.ebx = (mode) ? (mode->id | (1<<14)) : 3;
	bios_interrupt(0x10, &regs);
	if((regs.eax & 0xFF00) != 0) {
		kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
		return STATUS_DEVICE_ERROR;
	}

	return STATUS_SUCCESS;
}

/** VBE display operations. */
static display_ops_t vbe_display_ops = {
	.set_mode = vbe_display_set_mode,
};

/** Convert a mode depth to a pixel format.
 * @param depth		Depth to convert.
 * @return		Pixel format. */
static pixel_format_t depth_to_format(uint16_t depth) {
	switch(depth) {
	case 8:
		return PIXEL_FORMAT_IDX8;
	case 16:
		return PIXEL_FORMAT_RGB16;
	case 24:
		return PIXEL_FORMAT_RGB24;
	case 32:
	default:
		return PIXEL_FORMAT_RGB32;
	}
}

/** Initialisation function for the VBE driver.
 * @return		Status code describing result of the operation. */
static status_t vbe_init(void) {
	phys_ptr_t mem_phys = ~((phys_ptr_t)0);
	size_t count = 0, i, mem_size = 0;
	vbe_mode_info_t *minfo = NULL;
	display_mode_t *modes = NULL;
	vbe_info_t *info = NULL;
	uint16_t *location;
	bios_regs_t regs;
	status_t ret;

	/* Detect VBE presence by trying to get controller information. */
	info = bios_mem_alloc(sizeof(vbe_info_t), MM_SLEEP);
	strncpy(info->vbe_signature, "VBE2", 4);
	bios_regs_init(&regs);
	regs.eax = VBE_FUNCTION_CONTROLLER_INFO;
	regs.edi = bios_mem_virt2phys(info);
	bios_interrupt(0x10, &regs);
	if((regs.eax & 0x00FF) != 0x4F) {
		kprintf(LOG_DEBUG, "vbe: VBE is not supported!\n");
		ret = STATUS_NOT_SUPPORTED;
		goto out;
	} else if((regs.eax & 0xFF00) != 0) {
		kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
		ret = STATUS_DEVICE_ERROR;
		goto out;
	}

	kprintf(LOG_NORMAL, "vbe: vbe presence was detected:\n");
	kprintf(LOG_NORMAL, " signature:    %s\n", info->vbe_signature);
	kprintf(LOG_NORMAL, " version:      0x%" PRIx16 "\n", info->vbe_version);
	kprintf(LOG_NORMAL, " capabilities: 0x%" PRIx32 "\n", info->capabilities);
	kprintf(LOG_NORMAL, " mode pointer: 0x%" PRIx32 "\n", info->video_mode_ptr);
	kprintf(LOG_NORMAL, " total memory: %" PRIu16 "KB\n", info->total_memory * 64);
	if(info->vbe_version >= 0x0200) {
		kprintf(LOG_NORMAL, " OEM revision: 0x%" PRIx16 "\n", info->oem_software_rev);
	}
	mem_size = (info->total_memory * 64) * 1024;

	location = bios_mem_phys2virt(SEGOFF2LIN(info->video_mode_ptr));
	if(!location) {
		ret = STATUS_DEVICE_ERROR;
		goto out;
	}

	/* Allocate a region to store the mode information structure in. */
	minfo = bios_mem_alloc(sizeof(vbe_mode_info_t), MM_SLEEP);

	/* Iterate through all the modes available. An ID of 0xFFFF indicates
	 * the end of the mode list. */
	for(i = 0; location[i] != 0xFFFF; i++) {
		/* Get information on the mode. */
		bios_regs_init(&regs);
		regs.eax = VBE_FUNCTION_MODE_INFO;
		regs.ecx = location[i];
		regs.edi = bios_mem_virt2phys(minfo);
		bios_interrupt(0x10, &regs);
		if((regs.eax & 0xFF00) != 0) {
			kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
			ret = STATUS_DEVICE_ERROR;
			goto out;
		}

		/* Check if the mode is suitable. */
		if(minfo->memory_model != 4 && minfo->memory_model != 6) {
			/* Not packed-pixel or direct colour. */
			continue;
		} else if(minfo->phys_base_ptr == 0) {
			/* Borked. */
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

		/* Add the mode to the mode array. To begin with, set offset to
		 * the full physical address. */
		modes = krealloc(modes, sizeof(display_mode_t) * (count + 1), MM_SLEEP);
		modes[count].id = location[i];
		modes[count].width = minfo->x_resolution;
		modes[count].height = minfo->y_resolution;
		modes[count].format = depth_to_format(minfo->bits_per_pixel);
		modes[count].offset = minfo->phys_base_ptr;
		count++;

		/* Try to guess the memory address. */
		if(minfo->phys_base_ptr < mem_phys) {
			mem_phys = minfo->phys_base_ptr;
		}
	}

	/* Now fix up mode offsets. */
	for(i = 0; i < count; i++) {
		modes[i].offset = modes[i].offset - mem_phys;
	}

	/* Set the cache mode on the framebuffer to WC. */
	phys_set_memory_type(mem_phys, ROUND_UP(mem_size, PAGE_SIZE), MEMORY_TYPE_WC);

	/* Add the display device. */
	ret = display_device_create(NULL, NULL, &vbe_display_ops, NULL, modes, count,
	                            mem_phys, mem_size, &vbe_display_device);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
out:
	if(minfo) {
		bios_mem_free(minfo, sizeof(vbe_mode_info_t));
	}
	if(info) {
		bios_mem_free(info, sizeof(vbe_info_t));
	}
	return ret;
}

/** Unloading function for the VBE driver.
 * @return		Status code describing result of the operation. */
static status_t vbe_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("vbe");
MODULE_DESC("VESA BIOS Extensions (VBE) display driver");
MODULE_FUNCS(vbe_init, vbe_unload);
MODULE_DEPS("bios", "display");
