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
 * @brief               PC console code.
 */

#include <arch/io.h>

#include <device/irq.h>

#include <lib/ansi_parser.h>
#include <lib/notifier.h>
#include <lib/string.h>

#include <mm/phys.h>

#include <pc/console.h>

#include <proc/thread.h>

#include <sync/condvar.h>
#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

KBOOT_VIDEO(KBOOT_VIDEO_LFB, 0, 0, 0);

#ifdef SERIAL_PORT

/** Serial port ANSI escape parser. */
static ansi_parser_t serial_ansi_parser;

#endif

/**
 * i8042 input functions.
 */

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
    static bool ctrl = false;
    static bool alt = false;
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
static console_in_ops_t i8042_console_in_ops = {
    .poll = i8042_console_poll,
};

__init_text void i8042_init(void) {
    /* Empty i8042 buffer. */
    while (in8(0x64) & 1)
        in8(0x60);
}

/**
 * Serial console operations.
 */

#if SERIAL_PORT

/** Write a character to the serial console.
 * @param ch            Character to print. */
static void serial_console_putc(char ch) {
    if (ch == '\n')
        serial_console_putc('\r');

    out8(SERIAL_PORT, ch);

    while (!(in8(SERIAL_PORT + 5) & (1 << 5)))
        ;
}

/** Read a character from the serial console.
 * @return              Character read, or 0 if none available. */
static uint16_t serial_console_poll(void) {
    unsigned char ch = in8(SERIAL_PORT + 6);
    if ((ch & ((1 << 4) | (1 << 5))) && ch != 0xff) {
        if (in8(SERIAL_PORT + 5) & 0x01) {
            ch = in8(SERIAL_PORT);

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
    }

    return 0;
}

/** Early initialization of the serial console.
 * @return              Whether the serial console is present. */
static bool serial_console_early_init(void) {
    /* Check whether the port is present. */
    uint8_t status = in8(SERIAL_PORT + 6);
    if (!(status & ((1 << 4) | (1 << 5))) || status == 0xff)
        return false;

    out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
    out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
    out8(SERIAL_PORT + 0, 0x01);  /* Set divisor to 1 (lo byte) 115200 baud */
    out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
    out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    out8(SERIAL_PORT + 2, 0xc7);  /* Enable FIFO, clear them, with 14-byte threshold */
    out8(SERIAL_PORT + 4, 0x0b);  /* IRQs enabled, RTS/DSR set */

    /* Wait for transmit to be empty. */
    while (!(in8(SERIAL_PORT + 5) & (1 << 5)))
        ;

    ansi_parser_init(&serial_ansi_parser);
    return true;
}

/** Serial port console output operations. */
static console_out_ops_t serial_console_out_ops = {
    .putc        = serial_console_putc,
    .putc_unsafe = serial_console_putc,
};

/** Serial console input operations. */
static console_in_ops_t serial_console_in_ops = {
    .poll = serial_console_poll,
};

#endif

/*
 * Initialization functions.
 */

/** Set up the debug console.
 * @param video         KBoot video tag. */
__init_text void platform_console_early_init(kboot_tag_video_t *video) {
    #ifdef SERIAL_PORT
        /* Register the serial console for debug output. */
        if (serial_console_early_init()) {
            debug_console.out = &serial_console_out_ops;
            debug_console.in = &serial_console_in_ops;
        }
    #endif

    /* Register the early keyboard input operations. */
    main_console.in = &i8042_console_in_ops;
}   
