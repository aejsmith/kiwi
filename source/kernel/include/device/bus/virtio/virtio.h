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
 * @brief               VirtIO bus manager.
 */

#pragma once

#include <device/bus.h>
#include <device/dma.h>

#include <device/bus/virtio/virtio_ids.h>
#include <device/bus/virtio/virtio_ring.h>

#include <kernel/device/bus/virtio.h>

#include <lib/utility.h>

struct virtio_device;

#define VIRTIO_MODULE_NAME "virtio"

extern bus_t virtio_bus;

/** VirtIO driver structure. */
typedef struct virtio_driver {
    bus_driver_t bus;

    uint16_t device_id;                 /**< Supported device ID. */

    /** Initialize a device that matched against this driver.
     * @param device        Device to initialize.
     * @return              Status code describing the result of the operation. */
    status_t (*init_device)(struct virtio_device *device);

    /**
     * Handles a used buffer from the device. This is called when the IRQ
     * handler detects a new buffer in the used ring. It is the responsibility
     * of the handler to free this descriptor or reuse it.
     *
     * @param device        Device to handle on.
     * @param queue         Queue index.
     * @param elem          Used ring element.
     */
    void (*handle_used)(struct virtio_device *device, uint16_t index, struct vring_used_elem *elem);
} virtio_driver_t;

DEFINE_CLASS_CAST(virtio_driver, bus_driver, bus);

/** Define module init/unload functions for a VirtIO driver.
 * @param driver        Driver to register. */
#define MODULE_VIRTIO_DRIVER(driver) \
    MODULE_BUS_DRIVER(virtio_bus, driver)

/** Implementation of a VirtIO transport. */
typedef struct virtio_transport {
    uint32_t queue_align;               /**< Alignment for queues. */
    uint32_t queue_addr_width;          /**< Supported queue address width. */

    /** Get device status.
     * @param device        Device to get status of.
     * @return              Current device status. */
    uint8_t (*get_status)(struct virtio_device *device);

    /** Set device status bits or reset device.
     * @param device        Device to set status of.
     * @param status        Status bits to set. If 0, resets the device.*/
    void (*set_status)(struct virtio_device *device, uint8_t status);

    /** Get supported host features.
     * @param device        Device to get features of.
     * @return              Supported host features. */
    uint32_t (*get_features)(struct virtio_device *device);

    /** Set supported driver features.
     * @param device        Device to set features of.
     * @param features      Supported driver features. */
    void (*set_features)(struct virtio_device *device, uint32_t features);

    /** Get the size of a queue.
     * @param device        Device to get from.
     * @param index         Queue index.
     * @return              Queue size, or 0 if the queue doesn't exist. */
    uint16_t (*get_queue_size)(struct virtio_device *device, uint16_t index);

    /** Enable a queue.
     * @param device        Device to enable queue for.
     * @param index         Queue index. */
    void (*enable_queue)(struct virtio_device *device, uint16_t index);

    /** Disable a queue.
     * @param device        Device to disable queue for.
     * @param index         Queue index. */
    void (*disable_queue)(struct virtio_device *device, uint16_t index);

    /** Notify of new buffers in a queue.
     * @param device        Device to notify.
     * @param index         Queue index. */
    void (*notify)(struct virtio_device *device, uint16_t index);

    /** Read from the device-specific configuration space.
     * @param device        Device to read from.
     * @param offset        Byte offset to read from.
     * @return              Value read. */
    uint8_t (*get_config)(struct virtio_device *device, uint32_t offset);
} virtio_transport_t;

#define VIRTIO_MAX_QUEUES   4

/** VirtIO queue structure. */
typedef struct virtio_queue {
    struct vring ring;

    /** List of free descriptors (chained with desc.next). */
    uint16_t free_list;                 /**< Free pointer (0xffff = null). */
    uint16_t free_count;                /**< Number of free descriptors. */

    /** Last seen used index (only accessed by IRQ handler). */
    uint16_t last_used;

    /** Memory allocation details. */
    void *mem_virt;
    dma_ptr_t mem_dma;
    phys_size_t mem_size;
} virtio_queue_t;

/** Returns the descriptor at the given index in a queue. */
static inline struct vring_desc *virtio_queue_desc(virtio_queue_t *queue, uint16_t desc_index) {
    return &queue->ring.desc[desc_index];
}

/** Returns the next descriptor index in a chain in a queue.
 * @return              Next descriptor index, or 0xffff for end of chain. */
static inline uint16_t virtio_queue_next(virtio_queue_t *queue, struct vring_desc *desc) {
    return (desc->flags & VRING_DESC_F_NEXT) ? desc->next : 0xffff;
}

extern struct vring_desc *virtio_queue_alloc(virtio_queue_t *queue, uint16_t *_desc_index);
extern struct vring_desc *virtio_queue_alloc_chain(virtio_queue_t *queue, uint16_t count, uint16_t *_start_index);
extern void virtio_queue_free(virtio_queue_t *queue, uint16_t desc_index);
extern void virtio_queue_submit(virtio_queue_t *queue, uint16_t desc_index);

/**
 * VirtIO device structure. Access to the device is not synchronized by generic
 * VirtIO functions - it is the responsibility of the device driver to implement
 * appropriate synchronization.
 */
typedef struct virtio_device {
    bus_device_t bus;

    /** To be filled in by the transport on initialization. */
    uint16_t device_id;                 /**< Device ID. */
    virtio_transport_t *transport;      /**< Transport implementation. */

    uint32_t host_features;             /**< Supported host features. */
    uint32_t driver_features;           /**< Driver supported features. */
    void *private;                      /**< Private data pointer for driver. */

    /** Device virtqueues. */
    virtio_queue_t queues[VIRTIO_MAX_QUEUES];
} virtio_device_t;

DEFINE_CLASS_CAST(virtio_device, bus_device, bus);

#define VIRTIO_F(bit)   (1u << bit)

/** Destroy a VirtIO device. */
static inline void virtio_device_destroy(virtio_device_t *device) {
    bus_device_destroy(&device->bus);
}

/**
 * Notifies the device of new buffers in a queue. No synchronization is
 * necessary for calls to this.
 *
 * @param device        Device to notify.
 * @param index         Queue index.
 */
static inline void virtio_device_notify(virtio_device_t *device, uint16_t index) {
    device->transport->notify(device, index);
}

extern void virtio_device_get_config(virtio_device_t *device, void *buf, uint32_t offset, uint32_t size);

extern void virtio_device_set_features(virtio_device_t *device, uint32_t features);
extern virtio_queue_t *virtio_device_alloc_queue(virtio_device_t *device, uint16_t index);
extern void virtio_device_free_queue(virtio_device_t *device, uint16_t index);
extern void virtio_device_reset(virtio_device_t *device);

extern void virtio_device_irq(virtio_device_t *device);

extern status_t virtio_create_device(device_t *parent, virtio_device_t *device);

/** Search for a driver for a newly-added VirtIO device.
 * @param device        Device that has been added. */
static inline void virtio_match_device(virtio_device_t *device) {
    bus_match_device(&virtio_bus, &device->bus);
}
