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
 * @brief               VirtIO network device driver.
 */

#include <device/bus/virtio/virtio.h>

#include <mm/malloc.h>

#include <kernel.h>

#include "virtio_net.h"

#define VIRTIO_NET_SUPPORTED_FEATURES   (VIRTIO_F(VIRTIO_NET_F_MAC))
#define VIRTIO_NET_REQUIRED_FEATURES    (VIRTIO_F(VIRTIO_NET_F_MAC))

enum {
    VIRTIO_NET_QUEUE_RX = 0,
    VIRTIO_NET_QUEUE_TX = 1,
};

typedef struct virtio_net_device {
    virtio_device_t *virtio;

    virtio_queue_t *rx_queue;
    virtio_queue_t *tx_queue;
} virtio_net_device_t;

static status_t virtio_net_init_device(virtio_device_t *virtio) {
    status_t ret;

    if ((virtio->host_features & VIRTIO_NET_REQUIRED_FEATURES) != VIRTIO_NET_REQUIRED_FEATURES) {
        kprintf(LOG_WARN, "virtio_net: device does not support required feature set\n");
        return STATUS_NOT_SUPPORTED;
    }

    virtio_net_device_t *device = kmalloc(sizeof(*device), MM_KERNEL | MM_ZERO);

    device->virtio = virtio;

    device_t *node;
    ret = device_create("virtio_net", virtio->bus.node, NULL, device, NULL, 0, &node);
    // TODO: destruction: ops for destroy
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "virtio_net: failed to create device: %d\n", ret);
        return ret;
    }

    device_kprintf(
        node, LOG_NOTICE, "initializing device (features: 0x%x)\n",
        virtio->host_features);

    uint32_t features = virtio->host_features & VIRTIO_NET_SUPPORTED_FEATURES;
    virtio_device_set_features(virtio, features);

    uint8_t mac[6];
    virtio_device_get_config(virtio, mac, offsetof(struct virtio_net_config, mac), sizeof(mac));

    device_kprintf(node, LOG_NOTICE, "MAC address is %pM\n", mac);

    /* Create virtqueues. */
    // TODO: Should probably only have these created while the network device is
    // up?
    device->rx_queue = virtio_device_alloc_queue(virtio, VIRTIO_NET_QUEUE_RX);
    device->tx_queue = virtio_device_alloc_queue(virtio, VIRTIO_NET_QUEUE_TX);

    if (!device->rx_queue || !device->tx_queue) {
        device_kprintf(node, LOG_ERROR, "failed to create virtqueues\n");
        device_destroy(node);
        return STATUS_DEVICE_ERROR;
    }

// TODO: Synchronization for VirtIO queue access. Probably need a lock on each
// queue?

    return STATUS_SUCCESS;
}

static virtio_driver_t virtio_net_driver = {
    .device_id   = VIRTIO_ID_NET,
    .init_device = virtio_net_init_device,
};

MODULE_NAME("virtio_net");
MODULE_DESC("VirtIO network device driver");
MODULE_DEPS(VIRTIO_MODULE_NAME);
MODULE_VIRTIO_DRIVER(virtio_net_driver);
