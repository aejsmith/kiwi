/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Display device interface.
 *
 * @note		At the moment, this is nowhere near a proper display
 *			device interface...
 */

#ifndef __DRIVERS_DISPLAY_H
#define __DRIVERS_DISPLAY_H

#ifdef KERNEL
# include <types.h>
#else
# include <kernel/types.h>
#endif

/** Pixel format of a graphics buffer. */
typedef enum pixel_format {
	PIXEL_FORMAT_ARGB32,			/**< ARGB,      32-bit, 4 bytes, 8:8:8:8. */
	PIXEL_FORMAT_BGRA32,			/**< BGRA,	32-bit, 4 bytes, 8:8:8:8. */
	PIXEL_FORMAT_RGB32,			/**< RGB,       32-bit, 4 bytes, 8:8:8. */
	PIXEL_FORMAT_BGR32,			/**< BGR,	32-bit, 4 bytes, 8:8:8. */
	PIXEL_FORMAT_RGB24,			/**< RGB,       24-bit, 3 bytes, 8:8:8. */
	PIXEL_FORMAT_BGR24,			/**< BGR,	24-bit, 3 bytes, 8:8:8. */
	PIXEL_FORMAT_ARGB16,			/**< ARGB,      16-bit, 2 bytes, 1:5:5:5. */
	PIXEL_FORMAT_BGRA16,			/**< BGRA,	16-bit, 2 bytes, 5:5:5:1. */
	PIXEL_FORMAT_RGB16,			/**< RGB,       16-bit, 2 bytes, 5:6:5. */
	PIXEL_FORMAT_BGR16,			/**< BGR,	16-bit, 2 bytes, 5:6:5. */
	PIXEL_FORMAT_RGB15,			/**< RGB,       15-bit, 2 bytes, 5:5:5. */
	PIXEL_FORMAT_BGR15,			/**< BGR,       15-bit, 2 bytes, 5:5:5. */
	PIXEL_FORMAT_IDX8,			/**< Indexed,   8-bit,  1 byte. */
	PIXEL_FORMAT_GREY8,			/**< Greyscale, 8-bit,  1 byte. */
} pixel_format_t;

/** Structure describing a display mode. */
typedef struct display_mode {
	uint16_t id;				/**< Mode ID. */
	uint16_t width;				/**< Width of mode (in pixels). */
	uint16_t height;			/**< Height of mode (in pixels). */
	pixel_format_t format;			/**< Format of the framebuffer. */
	offset_t offset;			/**< Offset into device memory of framebuffer. */
} display_mode_t;

/** Display device request types. */
#define DISPLAY_MODE_COUNT		32	/**< Get the number of display modes. */
#define DISPLAY_GET_MODES		33	/**< Get an array of display modes. */
#define DISPLAY_GET_PREFERRED_MODE	34	/**< Get the preferred mode. */
#define DISPLAY_SET_MODE		35	/**< Set the display mode. */

/** Display device event types. */
#define DISPLAY_EVENT_REDRAW		32	/**< Wait until a redraw is required. */

#ifdef KERNEL

#include <io/device.h>

#include <lib/notifier.h>

struct display_device;

/** Display device operations structure. */
typedef struct display_ops {
	/** Handler for device-specific requests.
	 * @note		This is called when a device request ID is
	 *			received that is greater than or equal to
	 *			DEVICE_CUSTOM_REQUEST_START.
	 * @param device	Device request is being made on.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param in_size	Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param out_sizep	Where to store output buffer size.
	 * @return		Status code describing result of operation. */
	status_t (*request)(struct display_device *device, unsigned request,
		const void *in, size_t in_size, void **outp,
		size_t *out_sizep);

	/** Set the display mode.
	 * @param device	Device to set mode of.
	 * @param mode		Mode structure for mode to set.
	 * @return		Status code describing result of operation. */
	status_t (*set_mode)(struct display_device *device, display_mode_t *mode);
} display_ops_t;

/** Structure describing a display device. */
typedef struct display_device {
	/** Internal information. */
	mutex_t lock;			/**< Lock to protect device. */
	int id;				/**< Display ID. */
	display_ops_t *ops;		/**< Device operations structure. */
	void *data;			/**< Driver data structure. */
	atomic_t open;			/**< Whether the device is open. */
	display_mode_t *curr_mode;	/**< Current mode. */
	notifier_t redraw_notifier;	/**< Notifier for display redraw. */
	bool redraw;			/**< Whether any redraw requests have been missed. */

	/** Information about the device. */
	display_mode_t *modes;		/**< Array of mode structures. */
	size_t count;			/**< Number of modes. */
	phys_ptr_t mem_phys;		/**< Physical framebuffer location. */
	size_t mem_size;		/**< Size of the framebuffer. */
} display_device_t;

extern status_t display_device_create(const char *name, device_t *parent,
	display_ops_t *ops, void *data, display_mode_t *modes, size_t count,
	phys_ptr_t mem_phys, size_t mem_size, device_t **devicep);

#endif /* KERNEL */
#endif /* __DRIVERS_DISPLAY_H */
