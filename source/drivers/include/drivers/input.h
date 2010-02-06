/* Kiwi input device class
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
 * @brief		Input device class.
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
#define INPUT_KB_SET_LEDS	32	/**< Set keyboard LED state. */

/** Structure describing LED state to set. */
typedef struct input_kb_led_state {
	bool caps;			/**< Caps Lock LED state. */
	bool num;			/**< Num Lock LED state. */
	bool scroll;			/**< Scroll Lock LED state. */
} input_kb_led_state_t;

#ifdef KERNEL

#include <io/device.h>

#include <lib/notifier.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

struct input_device;
struct input_type;

/** Size of an input device buffer. */
#define INPUT_BUFFER_SIZE	128

/** Keyboard device operations structure. */
typedef struct input_kb_ops {
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
	 * @return		Positive value on success, negative error code
	 *			on failure. */
	int (*request)(struct input_device *device, int request, void *in, size_t insz, void **outp, size_t *outszp);

	/** Set LED state.
	 * @param device	Device to set state of.
	 * @param state		State structure. */
	void (*set_leds)(struct input_device *device, input_kb_led_state_t *state);
} input_kb_ops_t;

/** Mouse device operations structure. */
typedef struct input_mouse_ops {
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
	 * @return		Positive value on success, negative error code
	 *			on failure. */
	int (*request)(struct input_device *device, int request, void *in, size_t insz, void **outp, size_t *outszp);
} input_mouse_ops_t;

/** Input device structure. */
typedef struct input_device {
	identifier_t id;		/**< Device ID. */
	device_t *device;		/**< Device tree entry. */
	device_t *alias;		/**< Alias if main device is under a different directory. */
	struct input_type *type;	/**< Input device type. */
	void *ops;			/**< Operations for the device. */
	void *data;			/**< Data for the device code. */
	atomic_t open;			/**< Whether the device is open. */

	spinlock_t lock;		/**< Input buffer lock. */
	semaphore_t sem;		/**< Semaphore to wait for input on. */
	size_t start;			/**< Start position in input buffer. */
	size_t size;			/**< Current size of input buffer. */
	notifier_t data_notifier;	/**< Data notifier. */

	/** Input data buffer. */
	uint8_t buffer[INPUT_BUFFER_SIZE];
} input_device_t;

extern void input_device_input(input_device_t *device, uint8_t value);

extern int input_device_create(const char *name, device_t *parent, uint8_t type,
                               uint8_t protocol, void *ops, void *data,
                               input_device_t **devicep);
extern int input_device_destroy(input_device_t *device);

#endif /* KERNEL */
#endif /* __DRIVERS_INPUT_H */
