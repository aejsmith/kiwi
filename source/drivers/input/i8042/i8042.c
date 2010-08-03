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
 * @brief		i8042 keyboard/mouse port driver.
 */

#include <arch/io.h>

#include <cpu/intr.h>

#include <drivers/input.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <module.h>
#include <status.h>

extern void arch_reboot(void);

/** Keyboard device structure. */
static device_t *i8042_keyboard_dev;

/** IRQ handler for i8042 keyboard.
 * @param num		IRQ number.
 * @param _device	Device pointer.
 * @param frame		Interrupt stack frame.
 * @return		IRQ status code. */
static irq_result_t i8042_irq_handler(unative_t num, void *_device, intr_frame_t *frame) {
	device_t *device = _device;
	uint8_t code;

	assert(device == i8042_keyboard_dev);

	if(!(in8(0x64) & (1<<0))) {
		return IRQ_UNHANDLED;
	}

	code = in8(0x60);

	/* Some debugging hooks to go into KDBG, etc. */
	switch(code) {
	case 59:
		/* F1 - Enter KDBG. */
		kdbg_enter(KDBG_ENTRY_USER, frame);
		break;
	case 60:
		/* F2 - Call fatal(). */
		fatal("User requested fatal error");
		break;
	case 61:
		/* F3 - Reboot. */
		arch_reboot();
		break;
	}

	input_device_input(device, code);
	return IRQ_HANDLED;
}

/** Destroy an i8042 keyboard device.
 * @param device	Device to destroy. */
static void i8042_keyboard_destroy(input_device_t *device) {
	irq_unregister(1, i8042_irq_handler, NULL, i8042_keyboard_dev);
}

/** i8042 keyboard device operations structure. */
static keyboard_ops_t i8042_keyboard_ops = {
	.destroy = i8042_keyboard_destroy,
};

/** Initialisation function for the i8042 driver.
 * @return		Status code describing result of the operation. */
static status_t i8042_init(void) {
	status_t ret;

	ret = keyboard_device_create(NULL, NULL, INPUT_PROTOCOL_AT, &i8042_keyboard_ops,
	                             NULL, &i8042_keyboard_dev);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	kprintf(LOG_DEBUG, "i8042: registered i8042 keyboard device %p(%s)\n",
	        i8042_keyboard_dev, i8042_keyboard_dev->name);

	ret = irq_register(1, i8042_irq_handler, NULL, i8042_keyboard_dev);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "i8042: could not register keyboard IRQ (%d)\n", ret);
		device_destroy(i8042_keyboard_dev);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Unloading function for the i8042 driver.
 * @return		Status code describing result of the operation. */
static status_t i8042_unload(void) {
	return device_destroy(i8042_keyboard_dev);
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard/mouse port driver");
MODULE_FUNCS(i8042_init, i8042_unload);
MODULE_DEPS("input");
