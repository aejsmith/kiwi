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
 * @brief               Serial port console implementation.
 */

#pragma once

#include <types.h>

struct kboot_tag_serial;

/** Serial port operations. */
typedef struct serial_port_ops {
    /** Early initialize the serial port.
     * @param serial        KBoot serial tag.
     * @return              Whether the port is supported by this driver. */
    bool (*early_init)(struct kboot_tag_serial *serial);

    /** Properly initialize the port after memory management setup. */
    void (*init)(void);

    /** Check whether the RX buffer is empty.
     * @return              Whether the RX buffer is empty. */
    bool (*rx_empty)(void);

    /** Read from a port (RX will be non-empty).
     * @return              Value read from the port. */
    uint8_t (*read)(void);

    /** Check whether the TX buffer is empty.
     * @return              Whether the TX buffer is empty. */
    bool (*tx_empty)(void);

    /** Write to a port (TX will be empty).
     * @param val           Value to write. */
    void (*write)(uint8_t val);
} serial_port_ops_t;

extern void serial_console_early_init(struct kboot_tag_serial *serial);
