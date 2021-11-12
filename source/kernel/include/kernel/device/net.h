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
 * @brief               Network device class interface.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** Network device class name. */
#define NET_DEVICE_CLASS_NAME       "net"

/** Maximum hardware address (MAC) length. */
#define NET_DEVICE_ADDR_MAX         6

/** Network device requests. */
enum {
    /** Brings up the network interface. */
    NET_DEVICE_REQUEST_UP           = DEVICE_CLASS_REQUEST_START + 0,

    /** Shuts down the network interface. */
    NET_DEVICE_REQUEST_DOWN         = DEVICE_CLASS_REQUEST_START + 1,
};

/** Network interface flags. */
enum {
    /** Interface is up. */
    NET_INTERFACE_UP = (1<<0),
};

__KERNEL_EXTERN_C_END
