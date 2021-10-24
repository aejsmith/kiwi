/*
 * Copyright (C) 2009-2021 Alex Smith
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
 *
 * This driver only really handles what you'd find on a modern PC with an
 * emulated i8042 controller - keyboard (translated to scan code set 1) in the
 * first port and mouse in the second port.
 */

#include <device/input/input.h>

#include <device/io.h>
#include <device/irq.h>

#include <lib/utility.h>

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

typedef struct i8042_device {
    input_device_t input;
} i8042_device_t;

typedef struct i8042_controller {
    device_t *node;

    i8042_device_t keyboard;
    i8042_device_t mouse;

    io_region_t io;

    /** Keyboard state. */
    unsigned extended;
    unsigned pause_index;
    bool ralt_down;
} i8042_controller_t;

static i8042_controller_t i8042_controller;

static const uint8_t pause_sequence[] = { 0x1d, 0x45 };

extern int32_t i8042_keycode_table[128][2];

/** Waits until data is available in the output buffer. */
static void i8042_wait_data(i8042_controller_t *controller) {
    /* Wait for at most a second. */
    for (unsigned i = 0; i < 1000; i++) {
        if (io_read8(controller->io, I8042_PORT_STATUS) & I8042_STATUS_OUTPUT)
            return;

        delay(msecs_to_nsecs(1));
    }

    device_kprintf(controller->node, LOG_WARN, "timed out while waiting for data\n");
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

    device_kprintf(controller->node, LOG_WARN, "timed out while waiting to write\n");
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

    if (code == 0xe0 || code == 0xe1) {
        controller->extended    = (code == 0xe1) ? 2 : 1;
        controller->pause_index = 0;
    } else {
        input_event_type_t type = (code & 0x80) ? INPUT_EVENT_KEY_UP : INPUT_EVENT_KEY_DOWN;
        code &= 0x7f;

        int32_t key = INPUT_KEY_UNKNOWN;

        /* Special case for the weird pause key sequence (only thing on 0xe1 we
         * need to handle). */
        if (controller->extended == 2) {
            if (pause_sequence[controller->pause_index] == code) {
                if (++controller->pause_index == array_size(pause_sequence)) {
                    key = INPUT_KEY_PAUSE;
                    controller->extended = 0;
                }
            } else {
                controller->extended = 0;
            }
        } else {
            key = i8042_keycode_table[code][controller->extended];
            controller->extended = 0;
        }

        /* RAlt + F* - debugging hooks to go into KDB, etc. */
        if (key == INPUT_KEY_RIGHT_ALT)
            controller->ralt_down = type == INPUT_EVENT_KEY_DOWN;
        if (controller->ralt_down) {
            switch (key) {
                case INPUT_KEY_F1:
                    /* F1 - Enter KDB. */
                    kdb_enter(KDB_REASON_USER, NULL);
                    break;
                case INPUT_KEY_F2:
                    /* F2 - Call fatal(). */
                    fatal("User requested fatal error");
                    break;
                case INPUT_KEY_F3:
                    /* F3 - Reboot. */
                    system_shutdown(SHUTDOWN_REBOOT);
                    break;
                case INPUT_KEY_F4:
                    /* F4 - Shutdown. */
                    system_shutdown(SHUTDOWN_POWEROFF);
                    break;
            }
        }

        if (key != INPUT_KEY_UNKNOWN) {
            input_event_t event;
            event.time  = system_time();
            event.type  = type;
            event.value = key;

            input_device_event(&controller->keyboard.input, &event);
        }
    }
}

static status_t i8042_controller_init(i8042_controller_t *controller) {
    status_t ret;

    memset(controller, 0, sizeof(*controller));

    ret = device_create_dir("i8042", device_bus_platform_dir, &controller->node);
    if (ret != STATUS_SUCCESS)
        return ret;

    controller->io = device_pio_map(controller->node, I8042_PORT_BASE, I8042_PORT_COUNT);
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

    ret = input_device_create_etc(
        &controller->keyboard.input, "keyboard", controller->node,
        INPUT_DEVICE_KEYBOARD);
    if (ret != STATUS_SUCCESS)
        goto err;

    ret = device_irq_register(
        controller->node, I8042_IRQ_KEYBOARD, i8042_keyboard_irq_early,
        i8042_keyboard_irq, controller);
    if (ret != STATUS_SUCCESS)
        goto err;

    device_publish(controller->node);
    input_device_publish(&controller->keyboard.input);

    return STATUS_SUCCESS;

err:
    if (controller->keyboard.input.node)
        input_device_destroy(&controller->keyboard.input);

    if (controller->mouse.input.node)
        input_device_destroy(&controller->mouse.input);

    device_destroy(controller->node);
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
