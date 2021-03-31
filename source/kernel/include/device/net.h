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
 * @brief               Network device class.
 */

#pragma once

#include <device/device.h>

#include <kernel/device/net.h>

#include <net/net.h>

struct net_device;
struct packet;

/** Type of a network device. */
typedef enum net_device_type {
    NET_DEVICE_ETHERNET,
} net_device_type_t;

/** Network device operations. */
typedef struct net_device_ops {
    /** Destroy the device.
     * @param device        Device to destroy. */
    void (*destroy)(struct net_device *device);

    /** Transmit a packet on the device.
     * @param device        Device to transmit on.
     * @param packet        Packet to transmit.
     * @return              Status code describing the result of the operation. */
    status_t (*transmit)(struct net_device *device, struct packet *packet);
} net_device_ops_t;

/** Network device structure. */
typedef struct net_device {
    device_t *node;                     /**< Device tree node. */

    net_device_type_t type;
    net_device_ops_t *ops;

    /** Hardware address. */
    uint8_t hw_addr[NET_DEVICE_ADDR_MAX];
    uint8_t hw_addr_len;

    /**
     * Device's Maximum Transmission Unit (MTU). This is minus any link layer
     * headers and is used by upper layers to determine maximum packet sizes.
     */
    uint32_t mtu;
} net_device_t;

/** Destroys a network device.
 * @see                 device_destroy().
 * @param device        Device to destroy. */
static inline status_t net_device_destroy(net_device_t *device) {
    return device_destroy(device->node);
}

extern status_t net_device_create_etc(net_device_t *device, const char *name, device_t *parent);
extern status_t net_device_create(net_device_t *device, device_t *parent);
extern status_t net_device_publish(net_device_t *device);

extern status_t net_device_class_init(void);
