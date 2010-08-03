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
 * @brief		Input device class.
 *
 * The kernel side of input handling is very simple - most of the work is
 * offloaded to userspace. All we do is publish a device with attributes to
 * specify what type of device it is and what protocol it is using, and provide
 * raw data from the device to read calls on the device. We also provide
 * requests to do things such as set keyboard LED state, etc.
 */

#ifndef __DRIVERS_INPUT_H
#define __DRIVERS_INPUT_H

#ifdef KERNEL
# include <types.h>
#else
# include <stdbool.h>
#endif

/** Input device type attribute values. */
#define INPUT_TYPE_KEYBOARD	0	/**< Keyboard. */
#define INPUT_TYPE_MOUSE	1	/**< Mouse. */

/** Input device protocol attribute values. */
#define INPUT_PROTOCOL_AT	0	/**< AT keyboard. */

/** Input device request codes. */
#define KEYBOARD_SET_LEDS	32	/**< Set keyboard LED state. */

/** Structure describing LED state to set. */
typedef struct keyboard_led_state {
	bool caps;			/**< Caps Lock LED state. */
	bool num;			/**< Num Lock LED state. */
	bool scroll;			/**< Scroll Lock LED state. */
} keyboard_led_state_t;

#ifdef KERNEL

#include <io/device.h>

#include <lib/notifier.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

struct input_device;

/** Size of an input device buffer. */
#define INPUT_BUFFER_SIZE	128

/** Keyboard device operations structure. */
typedef struct keyboard_ops {
	/** Destroy data associated with the device.
	 * @param device	Device to destroy. */
	void (*destroy)(struct input_device *device);

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
	status_t (*request)(struct input_device *device, int request, void *in,
	                    size_t insz, void **outp, size_t *outszp);

	/** Set LED state.
	 * @param device	Device to set state of.
	 * @param state		State structure. */
	void (*set_leds)(struct input_device *device, keyboard_led_state_t *state);
} keyboard_ops_t;

/** Mouse device operations structure. */
typedef struct mouse_ops {
	/** Destroy data associated with the device.
	 * @param device	Device to destroy.
	 * @return		Status code describing result of operation. */
	status_t (*destroy)(struct input_device *device);

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
	status_t (*request)(struct input_device *device, int request, void *in,
	                    size_t insz, void **outp, size_t *outszp);
} mouse_ops_t;

/** Input device structure. */
typedef struct input_device {
	int id;				/**< Device ID. */

	/** Operations for the device. */
	union {
		keyboard_ops_t *kops;
		mouse_ops_t *mops;
		void *ops;
	};

	void *data;			/**< Implementation-specific data pointer. */
	atomic_t open;			/**< Whether the device is open. */
	uint8_t type;			/**< Type of the device. */

	spinlock_t lock;		/**< Input buffer lock. */
	semaphore_t sem;		/**< Semaphore to wait for input on. */
	size_t start;			/**< Start position in input buffer. */
	size_t size;			/**< Current size of input buffer. */
	notifier_t data_notifier;	/**< Data notifier. */

	/** Input data buffer. */
	uint8_t buffer[INPUT_BUFFER_SIZE];
} input_device_t;

extern void input_device_input(device_t *device, uint8_t value);

extern status_t keyboard_device_create(const char *name, device_t *parent, uint8_t protocol,
                                       keyboard_ops_t *ops, void *data, device_t **devicep);
extern status_t mouse_device_create(const char *name, device_t *parent, uint8_t protocol,
                                    mouse_ops_t *ops, void *data, device_t **devicep);

#endif /* KERNEL */
#endif /* __DRIVERS_INPUT_H */
