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
 */

#include <device/bus/virtio/virtio_config.h>
#include <device/bus/virtio/virtio.h>

#include <lib/string.h>
#include <lib/utility.h>

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
 * Create a new VirtIO device. Called by the transport driver to create the
 * VirtIO device node under the device node on the bus that the device was
 * found on. This does not search for and initialize a driver for the device,
 * this is done by virtio_add_device().
 *
 * @param module        Owning module (transport driver).
 * @param parent        Parent device node.
 * @param device        VirtIO device structure created by the transport.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t virtio_create_device_impl(module_t *module, device_t *parent, virtio_device_t *device) {
    status_t ret;

    assert(device->device_id != 0);

    /* Allocate a node ID to give it a name. */
    uint32_t node_id = atomic_fetch_add(&next_virtio_node_id, 1);
    char name[16];
    snprintf(name, sizeof(name), "%" PRIu32, node_id);

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
    ret = device_alias_etc(module, name, virtio_bus.dir, device->bus.node, NULL);
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
    virtio_device_t *device = container_of(_device, virtio_device_t, bus);
    virtio_driver_t *driver = container_of(_driver, virtio_driver_t, bus);

    return driver->device_id == device->device_id;
}

/** Initialize a VirtIO device. */
static status_t virtio_bus_init_device(bus_device_t *_device, bus_driver_t *_driver) {
    virtio_device_t *device = container_of(_device, virtio_device_t, bus);
    virtio_driver_t *driver = container_of(_driver, virtio_driver_t, bus);

    /* Reset the device and acknowledge it. */
    device->transport->set_status(device, 0);
    device->transport->set_status(device, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

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
