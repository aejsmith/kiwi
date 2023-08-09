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
 * @brief               Network device class interface.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** Network device class name. */
#define NET_DEVICE_CLASS_NAME       "net"

/** Maximum hardware address (MAC) length. */
#define NET_DEVICE_ADDR_MAX         6

/** Type of a network device. */
typedef enum net_device_type {
    NET_DEVICE_ETHERNET     = 0,    /**< Ethernet. */
} net_device_type_t;

/** Network device requests. */
enum {
    /** Brings up the network interface. */
    NET_DEVICE_REQUEST_UP           = DEVICE_CLASS_REQUEST_START + 0,

    /** Shuts down the network interface. */
    NET_DEVICE_REQUEST_DOWN         = DEVICE_CLASS_REQUEST_START + 1,

    /**
     * Gets the network interface ID. This is an ID that is set each time the
     * interface is brought up and is unique for the lifetime of the system.
     *
     * Output:              Network interface ID (uint32_t).
     *
     * Errors:              STATUS_NET_DOWN if the interface is down.
     */
    NET_DEVICE_REQUEST_INTERFACE_ID = DEVICE_CLASS_REQUEST_START + 2,

    /**
     * Get the hardware address.
     *
     * Output:              Hardware address. Size based on the hardware type,
     *                      up to a maximum of NET_DEVICE_ADDR_MAX.
     */
    NET_DEVICE_REQUEST_HW_ADDR      = DEVICE_CLASS_REQUEST_START + 3,

    /**
     * Adds an address to the network interface.
     *
     * Input:               A net_interface_addr_*_t structure corresponding to
     *                      the address family to add an address for. The size
     *                      and content of this is determined from the 'family'
     *                      member at the start of the structure.
     *
     * Errors:              STATUS_ALREADY_EXISTS if the address already exists
     *                      on the interface.
     *                      STATUS_ADDR_NOT_SUPPORTED if the address family is
     *                      not supported.
     *                      STATUS_NET_DOWN if the interface is down.
     */
    NET_DEVICE_REQUEST_ADD_ADDR     = DEVICE_CLASS_REQUEST_START + 4,

    /**
     * Removes an address from the network interface.
     *
     * Input:               A net_interface_addr_*_t structure corresponding to
     *                      the address family to remove an address for. The
     *                      size and content of this is determined from the
     *                      'family' member at the start of the structure.
     *
     * Errors:              STATUS_NOT_FOUND if the address does not exist on
     *                      the interface.
     *                      STATUS_ADDR_NOT_SUPPORTED if the address family is
     *                      not supported.
     *                      STATUS_NET_DOWN if the interface is down.
     */
    NET_DEVICE_REQUEST_REMOVE_ADDR  = DEVICE_CLASS_REQUEST_START + 5,
};

__KERNEL_EXTERN_C_END
