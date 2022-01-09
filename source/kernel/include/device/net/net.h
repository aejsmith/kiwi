/*
 * Copyright (C) 2009-2022 Alex Smith
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

#include <net/interface.h>

struct net_device;
struct net_packet;

/** Network device operations. */
typedef struct net_device_ops {
    /** Destroys the device.
     * @param device        Device to destroy. */
    void (*destroy)(struct net_device *device);

    /**
     * Brings up the interface. This will only be called when the interface is
     * down. Device implementations can use this and the down() function to, for
     * example, only allocate buffers while the device is up and stop receiving
     * packets while the device is down.
     *
     * @param device        Device to bring up.
     *
     * @return              Status code describing the result of the operation.
     */
    status_t (*up)(struct net_device *device);

    /**
     * Shuts down the interface. This will only be called when the interface is
     * up.
     *
     * @see                 net_device_ops_t::up().
     *
     * @param device        Device to shut down.
     *
     * @return              Status code describing the result of the operation.
     */
    status_t (*down)(struct net_device *device);

    /** Transmits a packet on the device.
     * @param device        Device to transmit on.
     * @param packet        Packet to transmit.
     * @return              Status code describing the result of the operation. */
    status_t (*transmit)(struct net_device *device, struct net_packet *packet);
} net_device_ops_t;

/** Network device structure. */
typedef struct net_device {
    device_t *node;                     /**< Device tree node. */

    /**
     * Fields filled in by driver.
     */

    net_device_type_t type;
    const net_device_ops_t *ops;

    /** Hardware address. */
    uint8_t hw_addr[NET_DEVICE_ADDR_MAX];
    uint8_t hw_addr_len;

    /**
     * Device's Maximum Transmission Unit (MTU). This is minus any link layer
     * headers and is used by upper layers to determine maximum packet sizes.
     */
    uint32_t mtu;

    /**
     * Internal device state.
     */

    net_interface_t interface;
} net_device_t;

/** Get the owning device of a network interface. */
static inline net_device_t *net_device_from_interface(net_interface_t *interface) {
    return container_of(interface, net_device_t, interface);
}

/**
 * Called when a packet is received on a device. Not safe from interrupt context,
 * use a threaded IRQ handler to call this.
 *
 * @param device        Device the packet was received on.
 * @param packet        Packet that was received.
 */
static inline void net_device_receive(net_device_t *device, struct net_packet *packet) {
    net_interface_receive(&device->interface, packet);
}

/** Destroys a network device.
 * @see                 device_destroy().
 * @param device        Device to destroy. */
static inline status_t net_device_destroy(net_device_t *device) {
    return device_destroy(device->node);
}

extern status_t net_device_create_etc(net_device_t *device, const char *name, device_t *parent);
extern status_t net_device_create(net_device_t *device, device_t *parent);
extern void net_device_publish(net_device_t *device);

extern void net_device_class_init(void);
