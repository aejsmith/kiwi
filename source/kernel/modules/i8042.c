/* Kiwi i8042 port driver
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
 * @brief		i8042 port driver.
 */

#include <arch/arch.h>
#include <arch/io.h>

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <lib/utility.h>

#include <io/device.h>

#include <sync/semaphore.h>

#include <errors.h>
#include <fatal.h>
#include <kdbg.h>
#include <module.h>

/** Lower case keyboard layout - United Kingdom */
static const unsigned char i8042_kbd_layout[] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, 0, 0,
	'#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0
};

#define BUFLEN 1024

static unsigned char i8042_buffer[BUFLEN];
static int i8042_buffer_size = 0;
static SEMAPHORE_DECLARE(i8042_sem, 0);
static SPINLOCK_DECLARE(i8042_lock);

/** I8042 keyboard interrupt handler.
 * @param num		IRQ number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns INTR_HANDLED. */
static intr_result_t i8042_irq_handler(unative_t num, void *data, intr_frame_t *frame) {
	uint8_t code = in8(0x60);

	/* For now we ignore the extended code, we don't have any use for
	 * it right now. */
	if(code >= 0xe0) {
		return INTR_HANDLED;
	}

	if(code < ARRAYSZ(i8042_kbd_layout)) {
		if(i8042_buffer_size >= BUFLEN) {
			return INTR_HANDLED;
		}
		spinlock_lock(&i8042_lock, 0);
		i8042_buffer[i8042_buffer_size++] = i8042_kbd_layout[code];
		spinlock_unlock(&i8042_lock);
		semaphore_up(&i8042_sem);
	}
	return INTR_HANDLED;
}

static int i8042_read(device_t *device, void *_buf, size_t count, offset_t offset, size_t *bytesp) {
	unsigned char *buf = _buf;
	size_t i;

	for(i = 0; i < count; i++) {
		semaphore_down(&i8042_sem, 0);

		spinlock_lock(&i8042_lock, 0);
		buf[i] = i8042_buffer[0];
		memcpy(i8042_buffer, &i8042_buffer[1], BUFLEN - 1);
		i8042_buffer_size--;
		spinlock_unlock(&i8042_lock);
	}

	*bytesp = count;
	return 0;
}

static device_ops_t i8042_device_ops = {
	.read = i8042_read,
};

/** Initialize the i8042 port driver.
 * @return		0 on success, negative error code on failure. */
static int i8042_init(void) {
	device_dir_t *dir;
	device_t *dev;
	int ret;

	if((ret = irq_register(1, i8042_irq_handler, NULL)) != 0) {
		return ret;
	}

	/* Register devices with the input device layer. */
	if((ret = device_dir_create("/input", &dir)) != 0) {
		irq_unregister(1, i8042_irq_handler, NULL);
		return ret;
	} else if((ret = device_create("keyboard", dir, DEVICE_TYPE_INPUT, &i8042_device_ops, NULL, &dev)) != 0) {
		irq_unregister(1, i8042_irq_handler, NULL);
		return ret;
	}

	kprintf(LOG_DEBUG, "i8042: registered i8042 keyboard device %p\n", dev);

	/* Empty i8042 buffer. */
	while(in8(0x64) & 1) {
		in8(0x60);
	}

	return 0;
}

/** Deinitialize the i8042 driver.
 * @return		0 on success, negative error code on failure. */
static int i8042_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard driver");
MODULE_FUNCS(i8042_init, i8042_unload);
