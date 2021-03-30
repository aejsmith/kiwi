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
 * @brief               VirtIO bus manager.
 */

#pragma once

#include <device/bus.h>

#include <device/bus/virtio/virtio_ids.h>
#include <device/bus/virtio/virtio_ring.h>

#include <kernel/device/bus/virtio.h>

#include <sync/mutex.h>

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
} virtio_driver_t;

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

    /** Notify of new buffers in a queue.
     * @param device        Device to notify.
     * @param index         Queue index. */
    void (*notify)(struct virtio_device *device, uint16_t index);
} virtio_transport_t;

#define VIRTIO_MAX_QUEUES   4

/** VirtIO queue structure. */
typedef struct virtio_queue {
    struct vring ring;

    /** List of free descriptors (chained with desc.next). */
    uint16_t free_list;                 /**< Free pointer (0xffff = null). */
    uint16_t free_count;                /**< Number of free descriptors. */

    uint16_t last_used;                 /**< Last seen used index. */

    /** Memory allocation details. */
    phys_ptr_t mem_phys;
    phys_size_t mem_size;
} virtio_queue_t;

/** Return the descriptor at the given index in a queue. */
static inline struct vring_desc *virtio_queue_desc(virtio_queue_t *queue, uint16_t desc_index) {
    return &queue->ring.desc[desc_index];
}

/** Return the next descriptor in a chain in a queue. */
static inline struct vring_desc *virtio_queue_next(virtio_queue_t *queue, struct vring_desc *desc) {
    return virtio_queue_desc(queue, desc->next);
}

extern struct vring_desc *virtio_queue_alloc(virtio_queue_t *queue, uint16_t *_desc_index);
extern struct vring_desc *virtio_queue_alloc_chain(virtio_queue_t *queue, uint16_t count, uint16_t *_start_index);
extern void virtio_queue_free(virtio_queue_t *queue, uint16_t desc_index);
extern void virtio_queue_submit(virtio_queue_t *queue, uint16_t desc_index);

/** VirtIO device structure. */
typedef struct virtio_device {
    bus_device_t bus;

    /** To be filled in by the transport on initialization. */
    uint16_t device_id;                 /**< Device ID. */
    virtio_transport_t *transport;      /**< Transport implementation. */

    /**
     * Synchronizes access to the device registers - all transport operations
     * will be called with this held. This does not synchronize access to any
     * queues.
     */
    mutex_t lock;

    uint32_t host_features;             /**< Supported host features. */

    /** Device virtqueues. */
    virtio_queue_t queues[VIRTIO_MAX_QUEUES];
} virtio_device_t;

#define VIRTIO_F(bit)   (1u << bit)

/** Destroy a VirtIO device. */
static inline void virtio_device_destroy(virtio_device_t *device) {
    bus_device_destroy(&device->bus);
}

extern void virtio_device_set_features(virtio_device_t *device, uint32_t features);
extern virtio_queue_t *virtio_device_alloc_queue(virtio_device_t *device, uint16_t index);
extern void virtio_device_notify(virtio_device_t *device, uint16_t index);

extern status_t virtio_create_device(device_t *parent, virtio_device_t *device);

/** Search for a driver for a newly-added VirtIO device.
 * @param device        Device that has been added. */
static inline void virtio_match_device(virtio_device_t *device) {
    bus_match_device(&virtio_bus, &device->bus);
}
