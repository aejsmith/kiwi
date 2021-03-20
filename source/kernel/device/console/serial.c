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
 * @brief               Serial port console implementation.
 */

#include <arch/cpu.h>

#include <device/console/ns16550.h>
#include <device/console/pl011.h>
#include <device/console/serial.h>

#include <lib/ansi_parser.h>
#include <lib/utility.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>

static serial_port_ops_t *serial_ops;
static ansi_parser_t serial_ansi_parser;
static SPINLOCK_DEFINE(serial_lock);

static void serial_console_init(void) {
    if (serial_ops->init)
        serial_ops->init();
}

static void serial_console_putc_unsafe(char ch) {
    if (ch == '\n')
        serial_console_putc_unsafe('\r');

    serial_ops->write(ch);

    while (!serial_ops->tx_empty())
        arch_cpu_spin_hint();
}

static void serial_console_putc(char ch) {
    spinlock_lock(&serial_lock);
    serial_console_putc_unsafe(ch);
    spinlock_unlock(&serial_lock);
}

static uint16_t serial_console_poll(void) {
    if (!serial_ops->rx_empty()) {
        uint8_t ch = serial_ops->read();

        /* Convert CR to NL, and DEL to Backspace. */
        if (ch == '\r') {
            ch = '\n';
        } else if (ch == 0x7f) {
            ch = '\b';
        }

        /* Handle escape sequences. */
        uint16_t converted = ansi_parser_filter(&serial_ansi_parser, ch);
        if (converted)
            return converted;
    }

    return 0;
}

static console_out_ops_t serial_console_out_ops = {
    .init        = serial_console_init,
    .putc        = serial_console_putc,
    .putc_unsafe = serial_console_putc_unsafe,
};

static console_in_ops_t serial_console_in_ops = {
    .poll = serial_console_poll,
};

static serial_port_ops_t *serial_console_types[] = {
    #if CONFIG_DEVICE_CONSOLE_NS16550
        &ns16550_serial_port_ops,
    #endif
    #if CONFIG_DEVICE_CONSOLE_PL011
        &pl011_serial_port_ops,
    #endif
};

/** Set up a serial port as the debug console.
 * @param serial        KBoot serial console details. */
void serial_console_early_init(kboot_tag_serial_t *serial) {
    for (size_t i = 0; i < array_size(serial_console_types); i++) {
        if (serial_console_types[i]->early_init(serial)) {
            serial_ops = serial_console_types[i];

            ansi_parser_init(&serial_ansi_parser);

            debug_console.out = &serial_console_out_ops;
            debug_console.in  = &serial_console_in_ops;

            break;
        }
    }
}
