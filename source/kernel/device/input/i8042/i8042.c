/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               i8042 keyboard/mouse controller driver.
 *
 * References:
 *  - https://wiki.osdev.org/%228042%22_PS/2_Controller
 *  - https://wiki.osdev.org/Mouse_Input
 */

#include <device/input.h>
#include <device/io.h>
#include <device/irq.h>

#include <kdb.h>
#include <module.h>
#include <status.h>
#include <time.h>

#define I8042_PORT_BASE             0x60
#define I8042_PORT_COUNT            5
#define I8042_PORT_DATA             0
#define I8042_PORT_STATUS           4
#define I8042_PORT_COMMAND          4

#define I8042_STATUS_OUTPUT         (1<<0)
#define I8042_STATUS_INPUT          (1<<1)

#define I8042_COMMAND_READ_CONFIG   0x20
#define I8042_COMMAND_WRITE_CONFIG  0x60

#define I8042_CONFIG_INTERRUPT_1    (1<<0)
#define I8042_CONFIG_INTERRUPT_2    (1<<1)
#define I8042_CONFIG_SYSTEM         (1<<2)
#define I8042_CONFIG_CLOCK_1        (1<<4)
#define I8042_CONFIG_CLOCK_2        (1<<5)

#define I8042_IRQ_KEYBOARD          1
#define I8042_IRQ_MOUSE             12

typedef struct i8042_controller {
    device_t *device;
    input_device_t *keyboard;
    input_device_t *mouse;

    io_region_t io;
} i8042_controller_t;

static i8042_controller_t i8042_controller;

/** Waits until data is available in the output buffer. */
static void i8042_wait_data(i8042_controller_t *controller) {
    /* Wait for at most a second. */
    for (unsigned i = 0; i < 1000; i++) {
        if (io_read8(controller->io, I8042_PORT_STATUS) & I8042_STATUS_OUTPUT)
            return;

        delay(msecs_to_nsecs(1));
    }

    device_kprintf(controller->device, LOG_WARN, "timed out while waiting for data\n");
}

/** Reads from the i8042 status port. */
static uint8_t i8042_read_status(i8042_controller_t *controller) {
    return io_read8(controller->io, I8042_PORT_STATUS);
}

/** Waits until space is available in the input buffer. */
static void i8042_wait_write(i8042_controller_t *controller) {
    /* Wait for at most a second. */
    for (unsigned i = 0; i < 1000; i++) {
        if (!(i8042_read_status(controller) & I8042_STATUS_INPUT))
            return;

        delay(msecs_to_nsecs(1));
    }

    device_kprintf(controller->device, LOG_WARN, "timed out while waiting to write\n");
}

/** Reads from the i8042 data port. */
static uint8_t i8042_read_data(i8042_controller_t *controller, bool wait) {
    if (wait)
        i8042_wait_data(controller);

    return io_read8(controller->io, I8042_PORT_DATA);
}

/** Writes to the i8042 data port, waiting for space. */
static void i8042_write_data(i8042_controller_t *controller, uint8_t data) {
    i8042_wait_write(controller);
    io_write8(controller->io, I8042_PORT_DATA, data);
}

/** Writes to the i8042 command port, waiting for space. */
static void i8042_write_command(i8042_controller_t *controller, uint8_t cmd) {
    i8042_wait_write(controller);
    io_write8(controller->io, I8042_PORT_COMMAND, cmd);
}

static irq_status_t i8042_keyboard_irq_early(unsigned num, void *data) {
    i8042_controller_t *controller = data;

    uint8_t status = i8042_read_status(controller);
    return (status & I8042_STATUS_OUTPUT) ? IRQ_RUN_THREAD : IRQ_UNHANDLED;
}

static void i8042_keyboard_irq(unsigned num, void *data) {
    i8042_controller_t *controller = data;

    uint8_t code = i8042_read_data(controller, false);

    /* Some debugging hooks to go into KDB, etc. */
    switch (code) {
        case 59:
            /* F1 - Enter KDB. */
            kdb_enter(KDB_REASON_USER, NULL);
            break;
        case 60:
            /* F2 - Call fatal(). */
            fatal("User requested fatal error");
            break;
        case 61:
            /* F3 - Reboot. */
            system_shutdown(SHUTDOWN_REBOOT);
            break;
        case 62:
            /* F4 - Shutdown. */
            system_shutdown(SHUTDOWN_POWEROFF);
            break;
    }

    device_kprintf(controller->device, LOG_DEBUG, "got code 0x%x\n", code);
}

static status_t i8042_controller_init(i8042_controller_t *controller) {
    status_t ret;

    controller->keyboard = NULL;
    controller->mouse    = NULL;

    ret = device_create_dir("i8042", device_bus_platform_dir, &controller->device);
    if (ret != STATUS_SUCCESS)
        return ret;

    controller->io = device_pio_map(controller->device, I8042_PORT_BASE, I8042_PORT_COUNT);
    if (controller->io == IO_REGION_INVALID)
        goto err;

    /* Empty output buffer (available data). */
    while (io_read8(controller->io, I8042_PORT_STATUS) & I8042_STATUS_OUTPUT)
        io_read8(controller->io, I8042_PORT_DATA);

    /* Set the controller configuration. */
    i8042_write_command(controller, I8042_COMMAND_READ_CONFIG);
    uint8_t config = i8042_read_data(controller, true);

    // TODO: Enable mouse interrupt when adding support.
    config |= I8042_CONFIG_INTERRUPT_1 | I8042_CONFIG_SYSTEM;
    config &= ~(I8042_CONFIG_CLOCK_1 | I8042_CONFIG_CLOCK_2);

    i8042_write_command(controller, I8042_COMMAND_WRITE_CONFIG);
    i8042_write_data(controller, config);

    ret = input_device_create(
        "keyboard", controller->device, INPUT_DEVICE_KEYBOARD,
        &controller->keyboard);
    if (ret != STATUS_SUCCESS)
        goto err;

    ret = device_irq_register(
        controller->device, I8042_IRQ_KEYBOARD, i8042_keyboard_irq_early,
        i8042_keyboard_irq, controller);
    if (ret != STATUS_SUCCESS)
        goto err;

    return STATUS_SUCCESS;

err:
    if (controller->keyboard)
        input_device_destroy(controller->keyboard);

    if (controller->mouse)
        input_device_destroy(controller->mouse);

    device_destroy(controller->device);
    return ret;
}

static status_t i8042_init(void) {
    return i8042_controller_init(&i8042_controller);
}

static status_t i8042_unload(void) {
    return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard/mouse controller driver");
MODULE_DEPS(INPUT_MODULE_NAME);
MODULE_FUNCS(i8042_init, i8042_unload);
