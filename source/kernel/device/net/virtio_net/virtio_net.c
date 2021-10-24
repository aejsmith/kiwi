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
    phys_ptr_t buf_phys;
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

    desc->addr  = queue->buf_phys + (desc_index * VIRTIO_BUFFER_SIZE);
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
    kprintf(LOG_DEBUG, "requeue buffer %u\n", desc_index);
    virtio_net_queue_rx(device, desc_index, true);
}

/** Destroys a VirtIO network device. */
static void virtio_net_device_destroy(net_device_t *net) {
    // TODO. Must handle partial destruction (init failure)
    fatal("TODO");
}

/** Transmits a packet on a VirtIO network device. */
static status_t virtio_net_device_transmit(net_device_t *_device, net_packet_t *packet) {
    virtio_net_device_t *device = cast_virtio_net_device(_device);

    assert(packet->size >= ETHERNET_MIN_FRAME_SIZE);
    assert(packet->size <= ETHERNET_MAX_FRAME_SIZE);

    virtio_net_queue_t *queue = &device->queues[VIRTIO_NET_QUEUE_TX];
    mutex_lock(&queue->lock);

    /* Allocate a descriptor. */
    uint16_t desc_index;
    struct vring_desc *desc = virtio_queue_alloc(queue->queue, &desc_index);
    if (!desc) {
        // TODO: Add this to a queue to process in the IRQ handler when a
        // descriptor becomes free.
        device_kprintf(device->net.node, LOG_WARN, "no TX descriptors free, dropping (TODO)\n");
        mutex_unlock(&queue->lock);
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

    desc->addr = queue->buf_phys + offset;
    desc->len  = packet->size + sizeof(*header);

    /* Submit the packet. */
    virtio_queue_submit(queue->queue, desc_index);
    virtio_device_notify(device->virtio, VIRTIO_NET_QUEUE_TX);

    mutex_unlock(&queue->lock);
    return STATUS_SUCCESS;
}

static const net_device_ops_t virtio_net_device_ops = {
    .destroy  = virtio_net_device_destroy,
    .transmit = virtio_net_device_transmit,
};

/** Handles a used buffer from a VirtIO network device. */
static void virtio_net_handle_used(virtio_device_t *virtio, uint16_t index, struct vring_used_elem *elem) {
    virtio_net_device_t *device = virtio->private;
    virtio_net_queue_t *queue   = &device->queues[index];

    mutex_lock(&queue->lock);

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

            kprintf(LOG_DEBUG, "packed received %u len %u\n", desc_index, size);
#if 0
            {
                ethernet_header_t *header = net_packet_data(packet, 0, sizeof(ethernet_header_t));
                assert(header);

                kprintf(LOG_DEBUG, "Eth: dest = %pM source = %pM type = 0x%x\n",
                    header->dest, header->source, net16_to_cpu(header->type));

                net_packet_offset(packet, sizeof(*header));

                arp_packet_t *arp = net_packet_data(packet, 0, sizeof(arp_packet_t));
                assert(arp);

                kprintf(LOG_DEBUG, "ARP: opcode = %u hw_sender = %pM proto_sender = %pI4 hw_target = %pM proto_target = %pI4\n",
                    net16_to_cpu(arp->opcode), arp->hw_sender, arp->proto_sender.bytes, arp->hw_target, arp->proto_target.bytes);
            }
#endif

            // TODO...
            net_packet_release(packet);
        } else {
            /* Transmit. Just free this descriptor. */
            kprintf(LOG_DEBUG, "free TX desc\n");
            virtio_queue_free(queue->queue, desc_index);
        }
    }

    mutex_unlock(&queue->lock);
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
    device->net.hw_addr_len = ETHERNET_ADDR_LEN;
    virtio_device_get_config(
        virtio, device->net.hw_addr,
        offsetof(struct virtio_net_config, mac), sizeof(device->net.hw_addr));

    device_kprintf(device->net.node, LOG_NOTICE, "MAC address is %pM\n", device->net.hw_addr);

    /* Create virtqueues and buffers. */
    // TODO: Should only have these created while the network device is up?
    for (unsigned i = 0; i < VIRTIO_NET_QUEUE_COUNT; i++) {
        virtio_net_queue_t *queue = &device->queues[i];

        mutex_init(
            &queue->lock,
            (i == VIRTIO_NET_QUEUE_RX) ? "virtio_net_rx_lock" : "virtio_net_tx_lock",
            MUTEX_RECURSIVE);

        /* Once we create the queue we can start getting interrupts off it. */
        mutex_lock(&queue->lock);

        queue->queue = virtio_device_alloc_queue(virtio, i);
        if (!queue->queue) {
            device_kprintf(device->net.node, LOG_ERROR, "failed to create virtqueues\n");
            mutex_unlock(&queue->lock);
            net_device_destroy(&device->net);
            return STATUS_DEVICE_ERROR;
        }

        uint16_t desc_count = queue->queue->ring.num;

        queue->buf_size = round_up(desc_count * VIRTIO_BUFFER_SIZE, PAGE_SIZE);

        device_kprintf(
            device->net.node, LOG_DEBUG,
            "%s queue has %" PRIu16 " descriptors (%zuKiB)\n",
            (i == VIRTIO_NET_QUEUE_RX) ? "RX" : "TX",
            desc_count, queue->buf_size / 1024);

        phys_alloc(queue->buf_size, 0, 0, 0, 0, MM_KERNEL, &queue->buf_phys);
        queue->buf_virt = phys_map(queue->buf_phys, queue->buf_size, MM_KERNEL);

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
    virtio_device_notify(virtio, VIRTIO_NET_QUEUE_RX);

    net_device_publish(&device->net);

#if 0
    {
        size_t size = max(sizeof(arp_packet_t), (ETHERNET_MIN_FRAME_SIZE - sizeof(ethernet_header_t)));

        arp_packet_t *data;
        net_packet_t *packet = net_packet_kmalloc(size, MM_KERNEL | MM_ZERO, (void **)&data);

        data->hw_type    = cpu_to_net16(ARP_HW_TYPE_ETHERNET);
        data->proto_type = cpu_to_net16(ETHERNET_TYPE_IPV4);
        data->hw_len     = ETHERNET_ADDR_LEN;
        data->proto_len  = IPV4_ADDR_LEN;
        data->opcode     = cpu_to_net16(ARP_OPCODE_REQUEST);

        memcpy(data->hw_sender, device->net.hw_addr, sizeof(data->hw_sender));
        data->proto_sender.val = 0;

        memset(data->hw_target, 0, sizeof(data->hw_target));
        data->proto_target.bytes[0] = 10;
        data->proto_target.bytes[1] = 0;
        data->proto_target.bytes[2] = 2;
        data->proto_target.bytes[3] = 2;

        ethernet_header_t *header;
        net_buffer_t *eth = net_buffer_kmalloc(sizeof(ethernet_header_t), MM_KERNEL, (void **)&header);

        memcpy(header->source, device->net.hw_addr, sizeof(header->source));
        uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
        memcpy(header->dest, broadcast, sizeof(header->dest));
        header->type = cpu_to_net16(ETHERNET_TYPE_ARP);

        net_packet_prepend(packet, eth);

        ret = virtio_net_device_transmit(&device->net, packet);
        if (ret == STATUS_SUCCESS) {
            kprintf(LOG_DEBUG, "packet transmitted!\n");
        } else {
            kprintf(LOG_DEBUG, "failed to transmit packet: %d\n", ret);
        }

        net_packet_release(packet);
    }
#endif

    return ret;
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
