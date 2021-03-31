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
 *
 * Reference:
 *  - Virtual I/O Device (VIRTIO) Version 1.1
 *    https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html
 *  - Virtio PCI Card Specification v0.9.5
 *    https://ozlabs.org/~rusty/virtio-spec/virtio-0.9.5.pdf
 *
 * TODO:
 *  - Implement proper support for destruction:
 *    - Ensure that the virtio_device_t gets destroyed when the parent (e.g.
 *      PCI) device gets removed.
 *    - Destroy queues and make sure the device is shut down when the child
 *      device gets destroyed.
 */

#include <arch/barrier.h>

#include <device/bus/virtio/virtio_config.h>
#include <device/bus/virtio/virtio.h>

#include <lib/string.h>

#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>

/** VirtIO device bus. */
__export bus_t virtio_bus;

/**
 * Next device node ID. Devices under the VirtIO bus directory are numbered
 * from this monotonically increasing ID. It has no real meaning since these
 * devices are all just aliases to the physical location of the devices on the
 * transport bus they were found on.
 */
static atomic_uint32_t next_virtio_node_id = 0;

/**
 * Queue management methods.
 */

/** Allocate a single descriptor from a queue.
 * @param queue         Queue to allocate for.
 * @param _desc_index   Where to store descriptor index.
 * @return              Pointer to descriptor, or NULL if none free. */
__export struct vring_desc *virtio_queue_alloc(virtio_queue_t *queue, uint16_t *_desc_index) {
    if (queue->free_count == 0)
        return NULL;

    uint16_t desc_index = queue->free_list;

    struct vring_desc *desc = &queue->ring.desc[desc_index];
    queue->free_list = desc->next;
    queue->free_count--;

    if (_desc_index)
        *_desc_index = desc_index;

    return desc;
}

/** Allocate a descriptor chain from a queue.
 * @param queue         Queue to allocate for.
 * @param count         Number of descriptors to allocate.
 * @param _start_index  Where to store start descriptor index.
 * @return              Pointer to descriptor, or NULL if none free. */
__export struct vring_desc *virtio_queue_alloc_chain(
    virtio_queue_t *queue, uint16_t count, uint16_t *_start_index)
{
    if (queue->free_count < count)
        return NULL;

    struct vring_desc *prev = NULL;
    uint16_t prev_index = 0;
    while (count > 0) {
        uint16_t desc_index;
        struct vring_desc *desc = virtio_queue_alloc(queue, &desc_index);

        desc->flags = (prev) ? VRING_DESC_F_NEXT : 0;
        desc->next  = (prev) ? prev_index : 0;

        prev       = desc;
        prev_index = desc_index;

        count--;
    }

    if (_start_index)
        *_start_index = prev_index;

    return prev;
}

/** Free a descriptor to a queue.
 * @param queue         Queue to free for.
 * @param index         Queue index.
 * @param desc          Descriptor index. */
__export void virtio_queue_free(virtio_queue_t *queue, uint16_t desc_index) {
    queue->ring.desc[desc_index].next = queue->free_list;
    queue->free_list                  = desc_index;

    queue->free_count++;
}

/** Submit a descriptor into a queue's available ring.
 * @param queue         Queue to submit to.
 * @param desc_index    Descriptor index. */
__export void virtio_queue_submit(virtio_queue_t *queue, uint16_t desc_index) {
    struct vring_avail *avail = queue->ring.avail;

    avail->ring[avail->idx % queue->ring.num] = desc_index;
    memory_barrier();
    avail->idx++;
    memory_barrier();
}

/**
 * Device methods.
 */

/** Read from the device-specific configuration space.
 * @param device        Device to read from.
 * @param buf           Buffer to read into.
 * @param offset        Byte offset to read from.
 * @param size          Number of bytes to read. */
__export void virtio_device_get_config(virtio_device_t *device, void *buf, uint32_t offset, uint32_t size) {
    uint8_t *data = buf;

    for (uint32_t i = 0; i < size; i++)
        data[i] = device->transport->get_config(device, offset + i);
}

/**
 * Sets the features supported by the driver. This must be a subset of the host
 * supported features. It must only be called during device init.
 *
 * @param device        Device to set for.
 * @param features      Features to enable.
 */
__export void virtio_device_set_features(virtio_device_t *device, uint32_t features) {
    assert((features & ~device->host_features) == 0);
    assert(!(device->transport->get_status(device) & VIRTIO_CONFIG_S_DRIVER_OK));

    device->transport->set_features(device, features);
}

/** Allocate and enable a queue (ring) for a VirtIO device.
 * @param device        Device to allocate for.
 * @param index         Queue index. The driver must not have previously
 *                      allocated this queue.
 * @return              Allocated queue, or NULL if the queue doesn't exist. */
__export virtio_queue_t *virtio_device_alloc_queue(virtio_device_t *device, uint16_t index) {
    assert(index < VIRTIO_MAX_QUEUES);

    virtio_queue_t *queue = &device->queues[index];
    assert(queue->mem_size == 0);

    uint16_t num_descs = device->transport->get_queue_size(device, index);
    if (num_descs == 0)
        return NULL;

    size_t align        = device->transport->queue_align;
    size_t mem_align    = round_up(align, PAGE_SIZE);
    phys_ptr_t max_addr = (phys_ptr_t)1 << device->transport->queue_addr_width;
    queue->mem_size     = round_up(vring_size(num_descs, align), mem_align);

    // TODO: Should we not use MM_WAIT here? Could be quite large.
    phys_alloc(queue->mem_size, mem_align, 0, 0, max_addr, MM_KERNEL, &queue->mem_phys);
    void *mem = phys_map_etc(
        queue->mem_phys, queue->mem_size,
        MMU_ACCESS_RW | MMU_CACHE_NORMAL, MM_KERNEL);

    memset(mem, 0, queue->mem_size);
    vring_init(&queue->ring, num_descs, mem, align);

    queue->last_used = 0;

    /* Add all descriptors to free list. */
    queue->free_list  = 0xffff;
    queue->free_count = 0;
    for (uint16_t i = 0; i < num_descs; i++)
        virtio_queue_free(queue, i);

    /* Enable the queue. */
    device->transport->enable_queue(device, index);

    return queue;
}

/**
 * Bus methods.
 */

/**
 * Create a new VirtIO device. Called by the transport driver to create the
 * VirtIO device node under the device node on the bus that the device was
 * found on. This does not search for and initialize a driver for the device,
 * this is done by virtio_add_device().
 *
 * @param parent        Parent device node.
 * @param device        VirtIO device structure created by the transport.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t virtio_create_device(device_t *parent, virtio_device_t *device) {
    status_t ret;

    assert(device->device_id != 0);

    bus_device_init(&device->bus);

    memset(device->queues, 0, sizeof(device->queues));

    /* Allocate a node ID to give it a name. */
    uint32_t node_id = atomic_fetch_add(&next_virtio_node_id, 1);
    char name[16];
    snprintf(name, sizeof(name), "%" PRIu32, node_id);

    module_t *module = module_caller();

    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS,            DEVICE_ATTR_STRING, { .string = VIRTIO_DEVICE_CLASS_NAME } },
        { VIRTIO_DEVICE_ATTR_DEVICE_ID, DEVICE_ATTR_UINT16, { .uint16 = device->device_id        } },
    };

    /* Create the device under the parent bus (physical location). */
    // TODO: destruction: needs ops to destroy the virtio_device_t.
    ret = device_create_etc(
        module, module->name, parent, NULL, &device->bus, attrs, array_size(attrs),
        &device->bus.node);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "virtio: failed to create device %s: %" PRId32, name, ret);
        return ret;
    }

    /* Alias it into the VirtIO bus. */
    ret = device_alias_etc(module_self(), name, virtio_bus.dir, device->bus.node, NULL);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "virtio: failed to create alias %s: %" PRId32, name, ret);
        // TODO: destruction - this is wrong since it would free virtio_device
        // but caller expects it to not be freed on failure.
        device_destroy(device->bus.node);
        return ret;
    }

    return STATUS_SUCCESS;
}

/** Match a VirtIO device to a driver. */
static bool virtio_bus_match_device(bus_device_t *_device, bus_driver_t *_driver) {
    virtio_device_t *device = cast_virtio_device(_device);
    virtio_driver_t *driver = cast_virtio_driver(_driver);

    return driver->device_id == device->device_id;
}

/** Initialize a VirtIO device. */
static status_t virtio_bus_init_device(bus_device_t *_device, bus_driver_t *_driver) {
    virtio_device_t *device = cast_virtio_device(_device);
    virtio_driver_t *driver = cast_virtio_driver(_driver);

    /* Reset the device and acknowledge it. */
    device->transport->set_status(device, 0);
    device->transport->set_status(device, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

    device->host_features = device->transport->get_features(device);

    /* Try to initialize the driver. */
    status_t ret = driver->init_device(device);

    /* Set status accordingly. */
    if (ret == STATUS_SUCCESS) {
        device->transport->set_status(device, VIRTIO_CONFIG_S_DRIVER_OK);
    } else {
        /* Set failed, but reset it immediately after. This should hopefully
         * stop the device from touching any rings that might have been set up
         * and allow us to free them. */
        device->transport->set_status(device, VIRTIO_CONFIG_S_FAILED);
        device->transport->set_status(device, 0);
    }

    return ret;
}

static bus_type_t virtio_bus_type = {
    .name         = "virtio",
    .device_class = VIRTIO_DEVICE_CLASS_NAME,
    .match_device = virtio_bus_match_device,
    .init_device  = virtio_bus_init_device,
};

static status_t virtio_init(void) {
    return bus_init(&virtio_bus, &virtio_bus_type);
}

static status_t virtio_unload(void) {
    return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("virtio");
MODULE_DESC("VirtIO bus manager");
MODULE_FUNCS(virtio_init, virtio_unload);
