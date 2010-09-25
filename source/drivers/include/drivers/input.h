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
 */

#ifndef __DRIVERS_INPUT_H
#define __DRIVERS_INPUT_H

#ifdef KERNEL
# include <types.h>
#else
# include <kernel/types.h>
#endif

/** Input event information structure. */
typedef struct input_event {
	useconds_t time;		/**< Time since boot that event occurred at. */
	uint8_t type;			/**< Event type. */
	int32_t value;			/**< Value. */
} input_event_t;

/** Input device type attribute values. */
#define INPUT_TYPE_KEYBOARD	0	/**< Keyboard. */
#define INPUT_TYPE_MOUSE	1	/**< Mouse. */

/** Input event types. */
#define INPUT_EVENT_KEY_DOWN	0	/**< Key down (keyboard). */
#define INPUT_EVENT_KEY_UP	1	/**< Key up (keyboard). */
#define INPUT_EVENT_REL_X	2	/**< Relative X movement (mouse). */
#define INPUT_EVENT_REL_Y	3	/**< Relative Y movement (mouse). */
#define INPUT_EVENT_BTN_DOWN	4	/**< Button down (mouse). */
#define INPUT_EVENT_BTN_UP	4	/**< Button up (mouse). */

/** Keyboard key codes. */
#define INPUT_KEY_LCTRL		1	/**< Left Ctrl. */
#define INPUT_KEY_LALT		2	/**< Left Alt. */
#define INPUT_KEY_LSUPER	3	/**< Left Super. */
#define INPUT_KEY_LSHIFT	4	/**< Left Shift. */
#define INPUT_KEY_RCTRL		5	/**< Right Ctrl. */
#define INPUT_KEY_RALT		6	/**< Right Alt. */
#define INPUT_KEY_RSUPER	7	/**< Right Super. */
#define INPUT_KEY_RSHIFT	8	/**< Right Shift. */
#define INPUT_KEY_CAPSLOCK	9	/**< Caps Lock. */
#define INPUT_KEY_SCROLLLOCK	10	/**< Scroll Lock. */
#define INPUT_KEY_NUMLOCK	11	/**< Num Lock. */
#define INPUT_KEY_ESC		12	/**< Escape. */
#define INPUT_KEY_F1		13	/**< F1. */
#define INPUT_KEY_F2		14	/**< F2. */
#define INPUT_KEY_F3		15	/**< F3. */
#define INPUT_KEY_F4		16	/**< F4. */
#define INPUT_KEY_F5		17	/**< F5. */
#define INPUT_KEY_F6		18	/**< F6. */
#define INPUT_KEY_F7		19	/**< F7. */
#define INPUT_KEY_F8		20	/**< F8. */
#define INPUT_KEY_F9		21	/**< F9. */
#define INPUT_KEY_F10		22	/**< F10. */
#define INPUT_KEY_F11		23	/**< F11. */
#define INPUT_KEY_F12		24	/**< F12. */
#define INPUT_KEY_PRSCRN	25	/**< Print Screen. */
#define INPUT_KEY_PAUSE		26	/**< Pause. */
#define INPUT_KEY_0		27	/**< 0. */
#define INPUT_KEY_1		28	/**< 1. */
#define INPUT_KEY_2		29	/**< 2. */
#define INPUT_KEY_3		30	/**< 3. */
#define INPUT_KEY_4		31	/**< 4. */
#define INPUT_KEY_5		32	/**< 5. */
#define INPUT_KEY_6		33	/**< 6. */
#define INPUT_KEY_7		34	/**< 7. */
#define INPUT_KEY_8		35	/**< 8. */
#define INPUT_KEY_9		36	/**< 9. */
#define INPUT_KEY_MINUS		37	/**< Minus. */
#define INPUT_KEY_EQUAL		38	/**< Equals. */
#define INPUT_KEY_BACKSPACE	39	/**< Backspace. */
#define INPUT_KEY_TAB		40	/**< Tab. */
#define INPUT_KEY_Q		41	/**< Q. */
#define INPUT_KEY_W		42	/**< W. */
#define INPUT_KEY_E		43	/**< E. */
#define INPUT_KEY_R		44	/**< R. */
#define INPUT_KEY_T		45	/**< T. */
#define INPUT_KEY_Y		46	/**< Y. */
#define INPUT_KEY_U		47	/**< U. */
#define INPUT_KEY_I		48	/**< I. */
#define INPUT_KEY_O		49	/**< O. */
#define INPUT_KEY_P		50	/**< P. */
#define INPUT_KEY_LBRACE	51	/**< Left Brace. */
#define INPUT_KEY_RBRACE	52	/**< Right Brace. */
#define INPUT_KEY_ENTER		53	/**< Enter. */
#define INPUT_KEY_A		54	/**< A. */
#define INPUT_KEY_S		55	/**< S. */
#define INPUT_KEY_D		56	/**< D. */
#define INPUT_KEY_F		57	/**< F. */
#define INPUT_KEY_G		58	/**< G. */
#define INPUT_KEY_H		59	/**< H. */
#define INPUT_KEY_J		60	/**< J. */
#define INPUT_KEY_K		61	/**< K. */
#define INPUT_KEY_L		62	/**< L. */
#define INPUT_KEY_SEMICOLON	63	/**< Semicolon. */
#define INPUT_KEY_APOSTROPHE	64	/**< Apostrophe. */
#define INPUT_KEY_BACKSLASH	65	/**< Backslash. */
#define INPUT_KEY_GRAVE		66	/**< Grave. */
#define INPUT_KEY_Z		67	/**< Z. */
#define INPUT_KEY_X		68	/**< X. */
#define INPUT_KEY_C		69	/**< C. */
#define INPUT_KEY_V		70	/**< V. */
#define INPUT_KEY_B		71	/**< B. */
#define INPUT_KEY_N		72	/**< N. */
#define INPUT_KEY_M		73	/**< M. */
#define INPUT_KEY_COMMA		74	/**< Comma. */
#define INPUT_KEY_PERIOD	75	/**< Period. */
#define INPUT_KEY_SLASH		76	/**< Forward Slash. */
#define INPUT_KEY_SPACE		77	/**< Space. */
#define INPUT_KEY_LEFT		78	/**< Left. */
#define INPUT_KEY_RIGHT		79	/**< Right. */
#define INPUT_KEY_UP		80	/**< Up. */
#define INPUT_KEY_DOWN		81	/**< Down. */
#define INPUT_KEY_INSERT	82	/**< Insert. */
#define INPUT_KEY_DELETE	83	/**< Delete. */
#define INPUT_KEY_HOME		84	/**< Home. */
#define INPUT_KEY_END		85	/**< End. */
#define INPUT_KEY_PGUP		86	/**< Page Up. */
#define INPUT_KEY_PGDOWN	87	/**< Page Down. */
#define INPUT_KEY_KPSLASH	88	/**< Keypad Slash. */
#define INPUT_KEY_KPASTERISK	89	/**< Keypad Asterisk. */
#define INPUT_KEY_KPMINUS	90	/**< Keypad Minus. */
#define INPUT_KEY_KPPLUS	91	/**< Keypad Plus. */
#define INPUT_KEY_KPENTER	92	/**< Keypad Enter. */
#define INPUT_KEY_KP7		93	/**< Keypad 7. */
#define INPUT_KEY_KP8		94	/**< Keypad 8. */
#define INPUT_KEY_KP9		95	/**< Keypad 9. */
#define INPUT_KEY_KP4		96	/**< Keypad 4. */
#define INPUT_KEY_KP5		97	/**< Keypad 5. */
#define INPUT_KEY_KP6		98	/**< Keypad 6. */
#define INPUT_KEY_KP1		99	/**< Keypad 1. */
#define INPUT_KEY_KP2		100	/**< Keypad 2. */
#define INPUT_KEY_KP3		101	/**< Keypad 3. */
#define INPUT_KEY_KP0		102	/**< Keypad 0. */
#define INPUT_KEY_KPPERIOD	103	/**< Keypad Period. */

/** Mouse buttons. */
#define INPUT_BUTTON_LEFT	0	/**< Left Button. */
#define INPUT_BUTTON_RIGHT	1	/**< Right Button. */
#define INPUT_BUTTON_MIDDLE	2	/**< Middle Button. */

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
} keyboard_ops_t;

/** Mouse device operations structure. */
typedef struct mouse_ops {
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

	/** Input event buffer. */
	input_event_t buffer[INPUT_BUFFER_SIZE];
} input_device_t;

extern void input_device_event(device_t *_device, uint8_t type, int32_t value);

extern status_t keyboard_device_create(const char *name, device_t *parent, keyboard_ops_t *ops,
                                       void *data, device_t **devicep);
extern status_t mouse_device_create(const char *name, device_t *parent, mouse_ops_t *ops,
                                    void *data, device_t **devicep);

#endif /* KERNEL */
#endif /* __DRIVERS_INPUT_H */
