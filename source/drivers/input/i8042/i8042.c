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
 * @brief		i8042 keyboard/mouse port driver.
 *
 * Reference:
 * - OSDev.org Wiki: Mouse Input
 *   http://wiki.osdev.org/Mouse_Input
 * - The PS/2 Mouse
 *   http://www.win.tue.nl/~aeb/linux/kbd/scancodes-13.html#ss13.3
 * - The AT Keyboard Controller
 *   http://www.win.tue.nl/~aeb/linux/kbd/scancodes-11.html#ss11.2
 */

#include <arch/io.h>

#include <device/irq.h>

#include <drivers/input.h>

#include <assert.h>
#include <console.h>
#include <dpc.h>
#include <kdbg.h>
#include <kernel.h>
#include <module.h>
#include <status.h>
#include <time.h>

/** Mouse settings. */
#define MOUSE_RATE		100	/**< Up to 100 packets per second. */
#define MOUSE_RESOLUTION	3	/**< 8 units per mm. */

extern int32_t i8042_keycode_table[128][2];

/** Registered i8042 devices. */
static device_t *i8042_keyboard_dev;
static device_t *i8042_mouse_dev;

/** Current keyboard state. */
static bool keyboard_seen_extended = false;

/** Current mouse state. */
static uint8_t mouse_button_state = 0;	/**< Mouse button states. */
static uint8_t mouse_packet[3];		/**< Packet read from mouse. */
static int mouse_packet_num = 0;	/**< Current packet byte number. */

/** Wait for bit 1 to clear in port 0x64. */
static inline void i8042_wait_write(void) {
	int i;

	/* Wait for at most a second. */
	for(i = 0; i < 1000; i++) {
		if(!(in8(0x64) & (1<<1))) {
			return;
		}
		usleep(1000);
	}

	kprintf(LOG_DEBUG, "i8042: warning: timed out while waiting to write\n");
}

/** Wait for bit 0 to become set in port 0x64. */
static inline void i8042_wait_data(void) {
	int i;

	/* Wait for at most a second. */
	for(i = 0; i < 1000; i++) {
		if(in8(0x64) & (1<<0)) {
			return;
		}
		usleep(1000);
	}

	kprintf(LOG_DEBUG, "i8042: warning: timed out while waiting for data\n");
}

/** Write to i8042 data port (0x60). */
static void i8042_data_write(uint8_t data) {
	i8042_wait_write();
	out8(0x60, data);
}

/** Write to i8042 command port (0x64). */
static void i8042_command_write(uint8_t cmd) {
	i8042_wait_write();
	out8(0x64, cmd);
}

/** IRQ handler for i8042 keyboard.
 * @param num		IRQ number.
 * @param _device	Device pointer.
 * @return		IRQ status code. */
static irq_status_t i8042_keyboard_irq(unsigned num, void *_device) {
	device_t *device = _device;
	uint8_t code, type;
	int32_t value;

	assert(device == i8042_keyboard_dev);

	if(!(in8(0x64) & (1<<0)) || in8(0x64) & (1<<5)) {
		return IRQ_UNHANDLED;
	}

	code = in8(0x60);

	/* Some debugging hooks to go into KDBG, etc. */
	switch(code) {
	case 59:
		/* F1 - Enter KDBG. */
		kdbg_enter(KDBG_ENTRY_USER, NULL);
		break;
	case 60:
		/* F2 - Call fatal(). */
		fatal("User requested fatal error");
		break;
	case 61:
		/* F3 - Reboot. */
		dpc_request((dpc_function_t)system_shutdown, (void *)((ptr_t)SHUTDOWN_REBOOT));
		break;
	case 62:
		/* F4 - Shutdown. */
		dpc_request((dpc_function_t)system_shutdown, (void *)((ptr_t)SHUTDOWN_POWEROFF));
		break;
	}

	/* If extended, set that we've seen an extended and return. */
	if(code >= 0xe0) {
		if(code == 0xe0) {
			keyboard_seen_extended = true;
		}
		return IRQ_HANDLED;
	}

	/* Convert key releases into the right event type. */
	if(code & 0x80) {
		code &= 0x7F;
		type = INPUT_EVENT_KEY_UP;
	} else {
		type = INPUT_EVENT_KEY_DOWN;
	}

	/* Translate the code into an input layer code. */
	value = i8042_keycode_table[code][keyboard_seen_extended];
	if(value) {
		input_device_event(device, type, value);
	}

	keyboard_seen_extended = false;
	return IRQ_HANDLED;
}

/** Destroy an i8042 keyboard device.
 * @param device	Device to destroy. */
static void i8042_keyboard_destroy(input_device_t *device) {
	irq_unregister(1, i8042_keyboard_irq, NULL, i8042_keyboard_dev);
}

/** i8042 keyboard device operations structure. */
static keyboard_ops_t i8042_keyboard_ops = {
	.destroy = i8042_keyboard_destroy,
};

/** Write a command to the mouse.
 * @param cmd		Command to write. */
static void i8042_mouse_command(uint8_t cmd) {
	/* Before writing the command to the data port, 0xD4 must be sent to
	 * the command port to make the command get sent to the mouse. */
	i8042_command_write(0xD4);
	i8042_data_write(cmd);

	/* Wait for an ACK on the data port. */
	i8042_wait_data();
	if(in8(0x60) != 0xFA) {
		kprintf(LOG_DEBUG, "i8042: warning: mouse command was not ACKed\n");
	}
}

/** IRQ handler for i8042 mouse.
 * @param num		IRQ number.
 * @param _device	Device pointer.
 * @return		IRQ status code. */
static irq_status_t i8042_mouse_irq(unsigned num, void *_device) {
	device_t *device = _device;
	uint8_t new_state = 0;
	int i, dx, dy;

	assert(device == i8042_mouse_dev);

	if(!(in8(0x64) & (1<<0)) || !(in8(0x64) & (1<<5))) {
		return IRQ_UNHANDLED;
	}

	mouse_packet[mouse_packet_num++] = in8(0x60);

	/* Check if a full packet has been received. */
	if(mouse_packet_num == 3) {
		/* Bit 3 of first byte should always be set. Use this to
		 * make sure we're in sync with what the mouse is sending. */
		if((mouse_packet[0] & (1<<3)) == 0) {
			mouse_packet[0] = mouse_packet[1];
			mouse_packet[1] = mouse_packet[2];
			mouse_packet_num = 2;
			return IRQ_HANDLED;
		}

		/* Check for position changes. Bit 4 in the first packet byte
		 * means that the X delta is negative, bit 5 means the Y delta
		 * is negative */
		dx = (mouse_packet[0] & (1<<4)) ? (int8_t)mouse_packet[1] : (uint8_t)mouse_packet[1];
		dy = (mouse_packet[0] & (1<<5)) ? (int8_t)mouse_packet[2] : (uint8_t)mouse_packet[2];

		/* Reverse the Y delta value. The mouse treats downward
		 * movement as negative, it makes more sense in most cases for
		 * it to be the opposite of this. */
		dy = -dy;

		/* Only add in events if there is a change. */
		if(dx != 0) {
			input_device_event(i8042_mouse_dev, INPUT_EVENT_REL_X, dx);
		}
		if(dy != 0) {
			input_device_event(i8042_mouse_dev, INPUT_EVENT_REL_Y, dy);
		}

		/* Check for changes in buttons. The button state is stored
		 * in the bottom 3 bits of the first packet byte. */
		new_state = mouse_packet[0] & 0x07;
		for(i = 0; i < 3; i++) {
			/* If the new state has the button, but the old state
			 * does not, it has been clicked. */
			if(new_state & (1<<i) && (mouse_button_state & (1<<i)) == 0) {
				input_device_event(device, INPUT_EVENT_BTN_DOWN, i);
			}

			/* If the old state has the button, but the new state
			 * does not, it has been released. */
			if((new_state & (1<<i)) == 0 && mouse_button_state & (1<<i)) {
				input_device_event(device, INPUT_EVENT_BTN_UP, i);
			}
		}

		/* Packet done, save new button state and reset to state 0. */
		mouse_button_state = new_state;
		mouse_packet_num = 0;
	}

	return IRQ_HANDLED;
}

/** Destroy an i8042 mouse device.
 * @param device	Device to destroy. */
static void i8042_mouse_destroy(input_device_t *device) {
	irq_unregister(12, i8042_mouse_irq, NULL, i8042_mouse_dev);
}

/** i8042 mouse device operations structure. */
static mouse_ops_t i8042_mouse_ops = {
	.destroy = i8042_mouse_destroy,
};

/** Initialisation function for the i8042 driver.
 * @return		Status code describing result of the operation. */
static status_t i8042_init(void) {
	uint8_t cmdbyte;
	status_t ret;

	/* Empty i8042 buffer. */
	while(in8(0x64) & 1) {
		in8(0x60);
	}

	/* Get the command byte from the controller. */
	i8042_command_write(0x20);
	i8042_wait_data();
	cmdbyte = in8(0x60);

	/* Enable keyboard/mouse interrupts, and set the System bit (bit 2) so
	 * that a reboot via the i8042 controller will be a "warm" reboot. */
	cmdbyte |= ((1<<0) | (1<<1) | (1<<2));
	cmdbyte &= ~((1<<4) | (1<<5));

	/* Write it back. */
	i8042_command_write(0x60);
	i8042_data_write(cmdbyte);

	/* Enable the AUX device. */
	i8042_command_write(0xA8);

	/* Set various parameters. */
	i8042_mouse_command(0xF3);
	i8042_mouse_command(MOUSE_RATE);
	i8042_mouse_command(0xE8);
	i8042_mouse_command(MOUSE_RESOLUTION);

	/* Enable the mouse. */
	i8042_mouse_command(0xF4);

	ret = keyboard_device_create(NULL, NULL, &i8042_keyboard_ops, NULL, &i8042_keyboard_dev);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	kprintf(LOG_DEBUG, "i8042: registered i8042 keyboard device %p(%s)\n",
	        i8042_keyboard_dev, i8042_keyboard_dev->name);

	ret = irq_register(1, i8042_keyboard_irq, NULL, i8042_keyboard_dev);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "i8042: could not register keyboard IRQ (%d)\n", ret);
		device_destroy(i8042_keyboard_dev);
		return ret;
	}

	ret = mouse_device_create(NULL, NULL, &i8042_mouse_ops, NULL, &i8042_mouse_dev);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	kprintf(LOG_DEBUG, "i8042: registered i8042 mouse device %p(%s)\n",
	        i8042_mouse_dev, i8042_mouse_dev->name);

	ret = irq_register(12, i8042_mouse_irq, NULL, i8042_mouse_dev);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "i8042: could not register mouse IRQ (%d)\n", ret);
		device_destroy(i8042_mouse_dev);
		device_destroy(i8042_keyboard_dev);
		return ret;
	}

	/* Empty i8042 buffer. */
	while(in8(0x64) & 1) {
		in8(0x60);
	}

	return STATUS_SUCCESS;
}

/** Unloading function for the i8042 driver.
 * @return		Status code describing result of the operation. */
static status_t i8042_unload(void) {
	status_t ret;

	ret = device_destroy(i8042_keyboard_dev);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = device_destroy(i8042_mouse_dev);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	return STATUS_SUCCESS;
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard/mouse port driver");
MODULE_FUNCS(i8042_init, i8042_unload);
MODULE_DEPS("input");
