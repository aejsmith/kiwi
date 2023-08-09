/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               AMD64 console code.
 */

#include <arch/io.h>

#include <x86/console.h>

#include <device/console/ns16550.h>

#include <lib/string.h>

#include <console.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>

#define SERIAL_PORT     0x3f8
#define SERIAL_CLOCK    1843200

/** Lower case keyboard layout - United Kingdom. */
static const unsigned char kbd_layout[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, 0, 0,
    '#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
    0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\'
};

/** Shift keyboard layout - United Kingdom. */
static const unsigned char kbd_layout_shift[128] = {
    0, 0, '!', '"', 156, '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', 0, 0,
    '~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
    0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '|'
};

/** Extended keyboard layout. */
static const uint16_t kbd_layout_extended[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CONSOLE_KEY_HOME, CONSOLE_KEY_UP, CONSOLE_KEY_PGUP, 0,
    CONSOLE_KEY_LEFT, 0, CONSOLE_KEY_RIGHT, 0, CONSOLE_KEY_END,
    CONSOLE_KEY_DOWN, CONSOLE_KEY_PGDN, 0, 0x7f
};

/** Translate a keycode read from the i8042 keyboard.
 * @param code          Keycode read.
 * @return              Translated character, or 0 if none available. */
static uint16_t i8042_console_translate(uint8_t code) {
    static bool shift = false;
    static bool ctrl __unused = false;
    static bool alt __unused = false;
    static bool extended = false;

    /* Check for an extended code. */
    if (code >= 0xe0) {
        if (code == 0xe0)
            extended = true;

        return 0;
    }

    /* Handle key releases. */
    if (code & 0x80) {
        code &= 0x7f;

        if (code == LEFT_SHIFT || code == RIGHT_SHIFT) {
            shift = false;
        } else if (code == LEFT_CTRL || code == RIGHT_CTRL) {
            ctrl = false;
        } else if (code == LEFT_ALT || code == RIGHT_ALT) {
            alt = false;
        }

        extended = false;
        return 0;
    }

    if (!extended) {
        if (code == LEFT_SHIFT || code == RIGHT_SHIFT) {
            shift = true;
            return 0;
        } else if (code == LEFT_CTRL || code == RIGHT_CTRL) {
            ctrl = true;
            return 0;
        } else if (code == LEFT_ALT || code == RIGHT_ALT) {
            alt = true;
            return 0;
        }
    }

    uint16_t ret = (extended)
        ? kbd_layout_extended[code]
        : ((shift) ? kbd_layout_shift[code] : kbd_layout[code]);

    extended = false;
    return ret;
}

/** Read a character from the i8042 keyboard. */
static uint16_t i8042_console_poll(void) {
    while (true) {
        /* Check for keyboard data. */
        uint8_t status = in8(0x64);
        if (status & (1 << 0)) {
            if (status & (1 << 5)) {
                /* Mouse data, discard. */
                in8(0x60);
                continue;
            }
        } else {
            return 0;
        }

        /* Read the code. */
        uint16_t ret = i8042_console_translate(in8(0x60));

        /* Little hack so that pressing Enter won't result in an extra newline
         * being sent. */
        if (ret == '\n') {
            while ((in8(0x64) & 1) == 0)
                ;

            in8(0x60);
        }

        return ret;
    }
}

/** i8042 console input operations. */
static const console_in_ops_t i8042_console_in_ops = {
    .poll = i8042_console_poll,
};

__init_text void i8042_init(void) {
    /* Empty i8042 buffer. */
    while (in8(0x64) & 1)
        in8(0x60);
}

/*
 * Initialization functions.
 */

/** Set up the debug console. */
__init_text void arch_console_early_init(kboot_tag_video_t *video, kboot_tag_serial_t *serial) {
    if (!serial) {
        /* Initialize and configure a serial port if the boot loader didn't
         * give us one. */
        kboot_tag_serial_t default_serial = {};
        default_serial.addr      = SERIAL_PORT;
        default_serial.io_type   = KBOOT_IO_TYPE_PIO;
        default_serial.type      = KBOOT_SERIAL_TYPE_NS16550;
        default_serial.baud_rate = 115200;
        default_serial.data_bits = 8;
        default_serial.stop_bits = 1;
        default_serial.parity    = KBOOT_SERIAL_PARITY_NONE;

        serial_console_early_init(&default_serial);

        ns16550_serial_configure(&default_serial, SERIAL_CLOCK);
    }

    /* Register the early keyboard input operations. */
    main_console.in = &i8042_console_in_ops;
}   
