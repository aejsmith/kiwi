/* Kiwi i8042 keyboard/mouse driver
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
 * @brief		i8042 keyboard/mouse port driver.
 */

#include <arch/arch.h>
#include <arch/io.h>

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <drivers/input.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>
#include <module.h>

/** Keyboard device structure. */
static input_device_t *i8042_kbd_dev;

/** IRQ handler for i8042 keyboard.
 * @param num		IRQ number.
 * @param _dev		Device pointer.
 * @param frame		Interrupt stack frame.
 * @return		IRQ status code. */
static irq_result_t i8042_kbd_handler(unative_t num, void *_dev, intr_frame_t *frame) {
	uint8_t code;

	assert(_dev == i8042_kbd_dev);

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

	input_device_input(i8042_kbd_dev, code);
	return IRQ_HANDLED;
}

/** Initialization function for the i8042 driver.
 * @return		0 on success, negative error code on failure. */
static int i8042_init(void) {
	int ret;

	if((ret = input_device_create(NULL, NULL, INPUT_TYPE_KEYBOARD, INPUT_PROTOCOL_PS2,
	                              NULL, NULL, &i8042_kbd_dev)) != 0) {
		return ret;
	}

	kprintf(LOG_DEBUG, "i8042: registered i8042 keyboard device %p(%d)\n", i8042_kbd_dev, i8042_kbd_dev->id);

	if((ret = irq_register(1, i8042_kbd_handler, NULL, i8042_kbd_dev)) != 0) {
		kprintf(LOG_WARN, "i8042: could not register keyboard IRQ (%d)\n", ret);
		input_device_destroy(i8042_kbd_dev);
		return ret;
	}

	return 0;
}

/** Unloading function for the i8042 driver.
 * @return		0 on success, negative error code on failure. */
static int i8042_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard/mouse port driver");
MODULE_FUNCS(i8042_init, i8042_unload);
MODULE_DEPS("input");
