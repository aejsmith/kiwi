/* Kiwi VBE display driver
 * Copyright (C) 2009 Alex Smith
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
 * @brief		VBE display driver.
 */

#include <console/kprintf.h>

#include <drivers/display.h>

#include <mm/malloc.h>

#include <platform/bios.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>
#include <module.h>

#include "vbe_priv.h"

/** Display device structure. */
static display_device_t *vbe_display_dev;

/** Get a framebuffer address.
 * @param _dev		Device to get address from.
 * @param offset	Offset into the framebuffer.
 * @param physp		Where to store physical address.
 * @return		0 on success, negative error code on failure. */
static int vbe_display_fault(display_device_t *_dev, offset_t offset, phys_ptr_t *physp) {
	vbe_device_t *device = _dev->data;

	if(offset > (offset_t)device->size || offset < 0) {
		return -ERR_NOT_FOUND;
	}

	*physp = device->phys + offset;
	return 0;
}

/** Set the display mode.
 * @param _dev		Device to set mode of.
 * @param mode		Mode structure for mode to set.
 * @return		0 on success, negative error code on failure. */
static int vbe_display_mode_set(display_device_t *_dev, display_mode_t *mode) {
	bios_regs_t regs;
	int ret;

	memset(&regs, 0, sizeof(bios_regs_t));

	if(mode) {
		kprintf(LOG_DEBUG, "vbe: switching to mode 0x%" PRIx32 " (mode: 0x%p)\n", mode->id, mode);
	}

	/* Set bit 14 in the mode register to use linear framebuffer model. */
	regs.eax = VBE_FUNCTION_SET_MODE;
	regs.ebx = (mode) ? (mode->id | (1<<14)) : 3;
	if((ret = bios_interrupt(0x10, &regs)) != 0) {
		return ret;
	} else if((regs.eax & 0xFF00) != 0) {
		kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
		return -ERR_DEVICE_ERROR;
	}

	return 0;
}

/** VBE display operations. */
static display_ops_t vbe_display_ops = {
	.fault = vbe_display_fault,
	.mode_set = vbe_display_mode_set,
};

/** Initialisation function for the VBE driver.
 * @return		0 on success, negative error code on failure. */
static int vbe_init(void) {
	vbe_device_t *device = kmalloc(sizeof(vbe_device_t), MM_SLEEP);
	vbe_mode_info_t *minfo = NULL;
	display_mode_t *modes = NULL;
	vbe_info_t *info = NULL;
	size_t count = 0, i;
	uint16_t *location;
	bios_regs_t regs;
	int ret;

	memset(&regs, 0, sizeof(bios_regs_t));

	/* Detect VBE presence by trying to get controller information. */
	info = bios_mem_alloc(sizeof(vbe_info_t), MM_SLEEP);
	strncpy(info->vbe_signature, "VBE2", 4);
	regs.eax = VBE_FUNCTION_CONTROLLER_INFO;
	regs.edi = bios_mem_virt2phys(info);
	if((ret = bios_interrupt(0x10, &regs)) != 0) {
		goto out;
	} else if((regs.eax & 0x00FF) != 0x4F) {
		kprintf(LOG_DEBUG, "vbe: VBE is not supported!\n");
		ret = -ERR_NOT_SUPPORTED;
		goto out;
	} else if((regs.eax & 0xFF00) != 0) {
		kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
		ret = -ERR_DEVICE_ERROR;
		goto out;
	}

	kprintf(LOG_DEBUG, "vbe: vbe presence was detected:\n");
	kprintf(LOG_DEBUG, " signature:    %s\n", info->vbe_signature);
	kprintf(LOG_DEBUG, " version:      0x%" PRIx16 "\n", info->vbe_version);
	kprintf(LOG_DEBUG, " capabilities: 0x%" PRIx32 "\n", info->capabilities);
	kprintf(LOG_DEBUG, " mode pointer: 0x%" PRIx32 "\n", info->video_mode_ptr);
	kprintf(LOG_DEBUG, " total memory: %" PRIu16 "KB\n", info->total_memory * 64);
	if(info->vbe_version >= 0x0200) {
		kprintf(LOG_DEBUG, " OEM revision: 0x%" PRIx16 "\n", info->oem_software_rev);
	}

	/* Save a copy of the data, but don't free it yet: the modes may be
	 * stored in the reserved section of it. */
	memcpy(&device->info, info, sizeof(vbe_info_t));
	device->phys = ~((phys_ptr_t)0);
	device->size = (info->total_memory * 64) * 1024;

	if(!(location = bios_mem_phys2virt(SEGOFF2LIN(device->info.video_mode_ptr)))) {
		ret = -ERR_DEVICE_ERROR;
		goto out;
	}

	/* Allocate a region to store the mode information structure in. */
	minfo = bios_mem_alloc(sizeof(vbe_mode_info_t), MM_SLEEP);

	/* Iterate through all the modes available. An ID of 0xFFFF indicates
	 * the end of the mode list. */
	for(i = 0; location[i] != 0xFFFF; i++) {
		memset(&regs, 0, sizeof(bios_regs_t));

		/* Get information on the mode. */
		regs.eax = VBE_FUNCTION_MODE_INFO;
		regs.ecx = location[i];
		regs.edi = bios_mem_virt2phys(minfo);
		if((ret = bios_interrupt(0x10, &regs)) != 0) {
			goto out;
		} else if((regs.eax & 0xFF00) != 0) {
			kprintf(LOG_DEBUG, "vbe: call failed with code 0x%x\n", regs.eax & 0xFFFF);
			ret = -ERR_DEVICE_ERROR;
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
		modes[count].bpp = minfo->bits_per_pixel;
		modes[count].offset = minfo->phys_base_ptr;
		kprintf(LOG_DEBUG, "vbe: detected mode 0x%" PRIx16 " 0x%" PRIx32 " (%dx%dx%d)\n",
			modes[count].id, modes[count].offset, modes[count].width, modes[count].height,
			modes[count].bpp);

		count++;

		/* Try to guess the memory address. */
		if(minfo->phys_base_ptr < device->phys) {
			device->phys = minfo->phys_base_ptr;
		}
	}

	/* Now fix up mode offsets. */
	for(i = 0; i < count; i++) {
		modes[i].offset = modes[i].offset - device->phys;
	}

	/* Add the display device. */
	if((ret = display_device_create(NULL, NULL, &vbe_display_ops, device, modes, count, &vbe_display_dev)) != 0) {
		return ret;
	}
out:
	if(minfo) {
		bios_mem_free(minfo, sizeof(vbe_mode_info_t));
	}
	if(info) {
		bios_mem_free(info, sizeof(vbe_info_t));
	}
	return 0;
}

/** Unloading function for the VBE driver.
 * @return		0 on success, negative error code on failure. */
static int vbe_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("vbe");
MODULE_DESC("VESA BIOS Extensions (VBE) display driver");
MODULE_FUNCS(vbe_init, vbe_unload);
MODULE_DEPS("bios", "display");
