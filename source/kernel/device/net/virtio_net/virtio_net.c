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
 *
 * TODO:
 *  - Checksum offloading.
 *  - Zero-copy transmit. Can use scatter-gather with descriptor chains, but
 *    all of the packet memory needs to be suitable allocations (we can deal
 *    with 64-bit physical addresses for VirtIO, but we should consider generic
 *    support for devices that have constraints on DMA, e.g. 32-bit addresses).
 */

#include <device/net/net.h>

#include <device/bus/virtio/virtio.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/phys.h>

#include <net/arp.h> // TODO: temp
#include <net/ethernet.h>
#include <net/packet.h>

#include <sync/mutex.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

#include "virtio_net.h"

struct virtio_net_device;

#define VIRTIO_NET_SUPPORTED_FEATURES   (VIRTIO_F(VIRTIO_NET_F_MAC))
#define VIRTIO_NET_REQUIRED_FEATURES    (VIRTIO_F(VIRTIO_NET_F_MAC))

/** Size of each RX/TX buffer to allocate.
 * @note                Will need to increase this by 2 if MRG_RXBUF is used. */
#define VIRTIO_BUFFER_SIZE \
    (ETHERNET_MAX_FRAME_SIZE + sizeof(struct virtio_net_hdr))

/** Queue indices. */
enum {
    VIRTIO_NET_QUEUE_RX = 0,
    VIRTIO_NET_QUEUE_TX = 1,

    VIRTIO_NET_QUEUE_COUNT
};

/** VirtIO network buffer structure. */
typedef struct virtio_net_buffer {
    net_buffer_external_t net;
    struct virtio_net_device *device;
} virtio_net_buffer_t;

/** RX/TX queue structure. */
typedef struct virtio_net_queue {
    mutex_t lock;
    virtio_queue_t *queue;

    size_t buf_size;
    dma_ptr_t buf_dma;
    uint8_t *buf_virt;
} virtio_net_queue_t;

/** VirtIO network device implementation. */
typedef struct virtio_net_device {
    net_device_t net;

    virtio_device_t *virtio;
    virtio_net_queue_t queues[VIRTIO_NET_QUEUE_COUNT];

    /**
     * Receive buffers, up-front allocated array indexed by RX queue descriptor
     * index.
     */
    virtio_net_buffer_t *rx_buffers;
} virtio_net_device_t;

DEFINE_CLASS_CAST(virtio_net_device, net_device, net);

/** Queues a descriptor in the available ring in the RX queue.
 * @param device        Device to queue on (RX queue locked).
 * @param desc_index    Descriptor index.
 * @param notify        Whether to notify the device. */
static void virtio_net_queue_rx(virtio_net_device_t *device, uint16_t desc_index, bool notify) {
    virtio_net_queue_t *queue = &device->queues[VIRTIO_NET_QUEUE_RX];

    struct vring_desc *desc = virtio_queue_desc(queue->queue, desc_index);

    desc->addr  = queue->buf_dma + (desc_index * VIRTIO_BUFFER_SIZE);
    desc->len   = VIRTIO_BUFFER_SIZE;
    desc->flags = VRING_DESC_F_WRITE;

    virtio_queue_submit(queue->queue, desc_index);

    if (notify)
        virtio_device_notify(device->virtio, VIRTIO_NET_QUEUE_RX);
}

/** Frees a VirtIO RX buffer. */
static void virtio_net_buffer_free(net_buffer_external_t *net) {
    virtio_net_buffer_t *buffer = (virtio_net_buffer_t *)net;
    virtio_net_device_t *device = buffer->device;

    uint16_t desc_index = buffer - device->rx_buffers;

    /* Re-queue the RX buffer for use again. */
    virtio_net_queue_rx(device, desc_index, true);
}

static void virtio_net_device_destroy(net_device_t *_device) {
    // TODO. Must handle partial destruction (init failure)
    fatal("TODO");
}

static status_t virtio_net_device_transmit(net_device_t *_device, net_packet_t *packet) {
    virtio_net_device_t *device = cast_virtio_net_device(_device);

    /* virtio-net can handle packets smaller than the minimum, no need to
     * manually pad. */
    assert(packet->size <= ETHERNET_MAX_FRAME_SIZE);

    virtio_net_queue_t *queue = &device->queues[VIRTIO_NET_QUEUE_TX];

    MUTEX_SCOPED_LOCK(lock, &queue->lock);

    /* Allocate a descriptor. */
    uint16_t desc_index;
    struct vring_desc *desc = virtio_queue_alloc(queue->queue, &desc_index);
    if (!desc) {
        // TODO: Add this to a queue to process in the IRQ handler when a
        // descriptor becomes free.
        device_kprintf(device->net.node, LOG_WARN, "no TX descriptors free, dropping (TODO)\n");
        return STATUS_DEVICE_ERROR;
    }

    /* Get descriptor data. */
    size_t offset = desc_index * VIRTIO_BUFFER_SIZE;
    uint8_t *data = queue->buf_virt + offset;

    /* Add the header. Nothing we need to care about right now so zero it. */
    struct virtio_net_hdr *header = (struct virtio_net_hdr *)data;
    memset(header, 0, sizeof(*header));

    /* Copy packet data. */
    net_packet_copy_from(packet, data + sizeof(*header), 0, packet->size);

    desc->addr = queue->buf_dma + offset;
    desc->len  = packet->size + sizeof(*header);

    /* Submit the packet. */
    virtio_queue_submit(queue->queue, desc_index);
    virtio_device_notify(device->virtio, VIRTIO_NET_QUEUE_TX);

    return STATUS_SUCCESS;
}

static status_t virtio_net_device_down(net_device_t *_device) {
    virtio_net_device_t *device = cast_virtio_net_device(_device);

    /* Shut down the queues. */
    for (unsigned i = 0; i < VIRTIO_NET_QUEUE_COUNT; i++) {
        virtio_net_queue_t *queue = &device->queues[i];

        mutex_lock(&queue->lock);

        if (queue->queue) {
            virtio_device_free_queue(device->virtio, i);
            queue->queue = NULL;

            dma_unmap(queue->buf_virt, queue->buf_size);
            dma_free(device->net.node, queue->buf_dma, queue->buf_size);
        }

        mutex_unlock(&queue->lock);
    }

    kfree(device->rx_buffers);
    device->rx_buffers = NULL;

    // TODO: Seems to be necessary otherwise the device doesn't work if we bring
    // it up again.
    virtio_device_reset(device->virtio);

    return STATUS_SUCCESS;
}

static status_t virtio_net_device_up(net_device_t *_device) {
    virtio_net_device_t *device = cast_virtio_net_device(_device);

    /* Create virtqueues and buffers. */
    for (unsigned i = 0; i < VIRTIO_NET_QUEUE_COUNT; i++) {
        virtio_net_queue_t *queue = &device->queues[i];

        /* Once we create the queue we can start getting interrupts off it. */
        mutex_lock(&queue->lock);

        queue->queue = virtio_device_alloc_queue(device->virtio, i);
        if (!queue->queue) {
            device_kprintf(device->net.node, LOG_ERROR, "failed to create virtqueues\n");
            mutex_unlock(&queue->lock);

            /* This will clean up what we've set up. */
            virtio_net_device_down(_device);
            return STATUS_DEVICE_ERROR;
        }

        uint16_t desc_count = queue->queue->ring.num;

        queue->buf_size = round_up(desc_count * VIRTIO_BUFFER_SIZE, PAGE_SIZE);

        device_kprintf(
            device->net.node, LOG_DEBUG,
            "%s queue has %" PRIu16 " descriptors (%zuKiB)\n",
            (i == VIRTIO_NET_QUEUE_RX) ? "RX" : "TX",
            desc_count, queue->buf_size / 1024);

        dma_alloc(device->net.node, queue->buf_size, NULL, MM_KERNEL, &queue->buf_dma);
        queue->buf_virt = dma_map(device->net.node, queue->buf_dma, queue->buf_size, MM_KERNEL);

        if (i == VIRTIO_NET_QUEUE_RX) {
            /* Create RX network buffers. */
            device->rx_buffers = kcalloc(desc_count, sizeof(*device->rx_buffers), MM_KERNEL);

            /* Queue up all available RX buffers to the device. */
            for (uint16_t j = 0; j < desc_count; j++) {
                uint16_t desc_index;
                struct vring_desc *desc = virtio_queue_alloc(queue->queue, &desc_index);
                assert(desc);

                virtio_net_queue_rx(device, desc_index, false);
            }
        }

        mutex_unlock(&queue->lock);
    }

    /* Notify the device that RX buffers are available. */
    virtio_device_notify(device->virtio, VIRTIO_NET_QUEUE_RX);

    return STATUS_SUCCESS;
}

static const net_device_ops_t virtio_net_device_ops = {
    .destroy  = virtio_net_device_destroy,
    .up       = virtio_net_device_up,
    .down     = virtio_net_device_down,
    .transmit = virtio_net_device_transmit,
};

/** Handles a used buffer from a VirtIO network device. */
static void virtio_net_handle_used(virtio_device_t *virtio, uint16_t index, struct vring_used_elem *elem) {
    virtio_net_device_t *device = virtio->private;
    virtio_net_queue_t *queue   = &device->queues[index];

    MUTEX_SCOPED_LOCK(lock, &queue->lock);

    /* Device has been shut down. */
    if (!queue->queue)
        return;

    uint16_t next_index = elem->id;
    while (next_index != 0xffff) {
        uint16_t desc_index = next_index;
        struct vring_desc *desc = virtio_queue_desc(queue->queue, desc_index);
        next_index = virtio_queue_next(queue->queue, desc);

        if (index == VIRTIO_NET_QUEUE_RX) {
            /* Receive. Construct a network buffer referring directly to the
             * received buffer data and pass it up to the network stack. The
             * descriptor will be added back to the available ring when the
             * packet is released. */
            if (desc->len <= sizeof(struct virtio_net_hdr)) {
                device_kprintf(device->net.node, LOG_WARN, "received buffer smaller than header, ignoring");
                virtio_net_queue_rx(device, desc_index, true);
                continue;
            }

            uint32_t size = desc->len - sizeof(struct virtio_net_hdr);
            uint8_t *data = queue->buf_virt + (desc_index * VIRTIO_BUFFER_SIZE) + sizeof(struct virtio_net_hdr);

            virtio_net_buffer_t *buffer = &device->rx_buffers[desc_index];

            net_buffer_init(&buffer->net.buffer);

            buffer->net.buffer.type = NET_BUFFER_TYPE_EXTERNAL;
            buffer->net.buffer.size = size;
            buffer->net.free        = virtio_net_buffer_free;
            buffer->net.data        = data;
            buffer->device          = device;

            net_packet_t *packet = net_packet_create(&buffer->net.buffer);

            net_device_receive(&device->net, packet);
            net_packet_release(packet);
        } else {
            /* Transmit. Just free this descriptor. */
            virtio_queue_free(queue->queue, desc_index);
        }
    }
}

/** Initializes a VirtIO network device. */
static status_t virtio_net_init_device(virtio_device_t *virtio) {
    status_t ret;

    virtio_net_device_t *device = kmalloc(sizeof(*device), MM_KERNEL | MM_ZERO);

    virtio->private = device;

    ret = net_device_create(&device->net, virtio->bus.node);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "virtio_net: failed to create device: %d\n", ret);
        kfree(device);
        return ret;
    }

    device_add_kalloc(device->net.node, device);

    device_kprintf(
        device->net.node, LOG_NOTICE, "initializing device (features: 0x%x)\n",
        virtio->host_features);

    device->net.type = NET_DEVICE_ETHERNET;
    device->net.ops  = &virtio_net_device_ops;
    device->net.mtu  = ETHERNET_MTU;
    device->virtio   = virtio;

    if ((virtio->host_features & VIRTIO_NET_REQUIRED_FEATURES) != VIRTIO_NET_REQUIRED_FEATURES) {
        device_kprintf(device->net.node, LOG_WARN, "virtio_net: device does not support required feature set\n");
        net_device_destroy(&device->net);
        return STATUS_NOT_SUPPORTED;
    }

    /* Tell the device the features we're using. */
    uint32_t features = virtio->host_features & VIRTIO_NET_SUPPORTED_FEATURES;
    virtio_device_set_features(virtio, features);

    /* Retrieve the MAC address. */
    device->net.hw_addr_len = ETHERNET_ADDR_SIZE;
    virtio_device_get_config(
        virtio, device->net.hw_addr,
        offsetof(struct virtio_net_config, mac), sizeof(device->net.hw_addr));

    device_kprintf(device->net.node, LOG_NOTICE, "MAC address is %pM\n", device->net.hw_addr);

    for (unsigned i = 0; i < VIRTIO_NET_QUEUE_COUNT; i++) {
        virtio_net_queue_t *queue = &device->queues[i];

        mutex_init(
            &queue->lock,
            (i == VIRTIO_NET_QUEUE_RX) ? "virtio_net_rx_lock" : "virtio_net_tx_lock",
            MUTEX_RECURSIVE);
    }

    net_device_publish(&device->net);
    return STATUS_SUCCESS;
}

static virtio_driver_t virtio_net_driver = {
    .device_id   = VIRTIO_ID_NET,
    .handle_used = virtio_net_handle_used,
    .init_device = virtio_net_init_device,
};

MODULE_NAME("virtio_net");
MODULE_DESC("VirtIO network device driver");
MODULE_DEPS(NET_MODULE_NAME, VIRTIO_MODULE_NAME);
MODULE_VIRTIO_DRIVER(virtio_net_driver);
