/*
 * Copyright (C) 2009-2010 Alex Smith
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

/** Structure describing a display mode. */
typedef struct display_mode {
	uint16_t id;			/**< Mode ID. */
	uint16_t width;			/**< Width of mode (in pixels). */
	uint16_t height;		/**< Height of mode (in pixels). */
	uint8_t depth;			/**< Bits per pixel. */
	offset_t offset;		/**< Offset into device memory of framebuffer. */
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
	 * @param insz		Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param outszp	Where to store output buffer size.
	 * @return		Status code describing result of operation. */
	status_t (*request)(struct display_device *device, int request, void *in, size_t insz,
	                    void **outp, size_t *outszp);

	/** Get a framebuffer address.
	 * @note		This should check that the offset is within the
	 *			device memory.
	 * @param device	Device to get address from.
	 * @param offset	Offset into the framebuffer.
	 * @param physp		Where to store physical address.
	 * @return		Status code describing result of operation. */
	status_t (*get_page)(struct display_device *device, offset_t offset, phys_ptr_t *physp);

	/** Set the display mode.
	 * @param device	Device to set mode of.
	 * @param mode		Mode structure for mode to set.
	 * @return		Status code describing result of operation. */
	status_t (*set_mode)(struct display_device *device, display_mode_t *mode);
} display_ops_t;

/** Structure describing a display device. */
typedef struct display_device {
	mutex_t lock;			/**< Lock to protect device. */
	int id;				/**< Device ID. */
	display_ops_t *ops;		/**< Device operations structure. */
	void *data;			/**< Driver data structure. */
	atomic_t open;			/**< Whether the device is open. */

	display_mode_t *modes;		/**< Array of mode structures. */
	size_t count;			/**< Number of modes. */

	display_mode_t *curr_mode;	/**< Current mode. */
	notifier_t redraw_notifier;	/**< Notifier for display redraw. */
	bool redraw;			/**< Whether any redraw requests have been missed. */
} display_device_t;

extern status_t display_device_create(const char *name, device_t *parent, display_ops_t *ops,
                                      void *data, display_mode_t *modes, size_t count,
                                      device_t **devicep);

#endif /* KERNEL */
#endif /* __DRIVERS_DISPLAY_H */
