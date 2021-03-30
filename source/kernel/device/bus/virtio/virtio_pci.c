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
 * @brief               VirtIO PCI transport driver.
 */

#include <device/bus/pci.h>

#include <device/bus/virtio/virtio.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kernel.h>

#include "virtio_pci.h"

/** VirtIO PCI device structure. */
typedef struct virtio_pci_device {
    virtio_device_t virtio;
    pci_device_t *pci;
    io_region_t io;
} virtio_pci_device_t;

static uint8_t virtio_pci_get_status(virtio_device_t *_device) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    return io_read8(device->io, VIRTIO_PCI_STATUS);
}

static void virtio_pci_set_status(virtio_device_t *_device, uint8_t status) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    if (status == 0) {
        io_write8(device->io, VIRTIO_PCI_STATUS, status);
    } else {
        uint8_t val = io_read8(device->io, VIRTIO_PCI_STATUS);
        val |= status;
        io_write8(device->io, VIRTIO_PCI_STATUS, val);
    }
}

static uint32_t virtio_pci_get_features(virtio_device_t *_device) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    return io_read32(device->io, VIRTIO_PCI_HOST_FEATURES);
}

static void virtio_pci_set_features(virtio_device_t *_device, uint32_t features) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    io_write32(device->io, VIRTIO_PCI_GUEST_FEATURES, features);
}

static uint16_t virtio_pci_get_queue_size(virtio_device_t *_device, uint16_t index) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    return io_read16(device->io, VIRTIO_PCI_QUEUE_NUM);
}

static void virtio_pci_enable_queue(virtio_device_t *_device, uint16_t index) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);
    virtio_queue_t *queue = &device->virtio.queues[index];

    io_write16(device->io, VIRTIO_PCI_QUEUE_SEL, index);
    io_write32(device->io, VIRTIO_PCI_QUEUE_PFN, queue->mem_phys >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}

static void virtio_pci_notify(virtio_device_t *_device, uint16_t index) {
    virtio_pci_device_t *device = container_of(_device, virtio_pci_device_t, virtio);

    io_write16(device->io, VIRTIO_PCI_QUEUE_NOTIFY, index);
}

static virtio_transport_t virtio_pci_transport = {
    .queue_align      = VIRTIO_PCI_VRING_ALIGN,
    .queue_addr_width = 32 + VIRTIO_PCI_QUEUE_ADDR_SHIFT,

    .get_status       = virtio_pci_get_status,
    .set_status       = virtio_pci_set_status,
    .get_features     = virtio_pci_get_features,
    .set_features     = virtio_pci_set_features,
    .get_queue_size   = virtio_pci_get_queue_size,
    .enable_queue     = virtio_pci_enable_queue,
    .notify           = virtio_pci_notify,
};

static status_t virtio_pci_init_device(pci_device_t *pci) {
    status_t ret;

    /* We only support legacy for now as this what most implementations are. */
    if (pci->revision != VIRTIO_PCI_ABI_VERSION) {
        kprintf(
            LOG_WARN, "virtio_pci: %s: non-legacy devices are not currently supported\n",
            pci->bus.node->name);
        return STATUS_NOT_SUPPORTED;
    }

    /* If the PCI device ID is not a transitional one we can use that, otherwise
     * for legacy devices the ID is in the PCI subsystem device ID.  */
    uint16_t device_id = (pci->device_id < 0x1040)
        ? pci_config_read16(pci, PCI_CONFIG_SUBSYS_ID)
        : pci->device_id - 0x1040;

    if (device_id == 0) {
        /* Reserved ID, just ignore. */
        return STATUS_SUCCESS;
    }

    kprintf(
        LOG_NOTICE, "virtio_pci: %s: detected device ID %" PRIu16 "\n",
        pci->bus.node->name, device_id);

    /* Create a VirtIO device. */
    virtio_pci_device_t *device = kmalloc(sizeof(*device), MM_KERNEL);

    device->virtio.device_id = device_id;
    device->virtio.transport = &virtio_pci_transport;
    device->pci              = pci;

    ret = virtio_create_device(pci->bus.node, &device->virtio);
    if (ret != STATUS_SUCCESS) {
        kfree(device);
        return ret;
    }

    /* Map the I/O region in BAR 0. */
    ret = device_pci_bar_map(device->virtio.bus.node, pci, 0, MM_KERNEL, &device->io);
    if (ret != STATUS_SUCCESS) {
        kprintf(
            LOG_NOTICE, "virtio_pci: %s: failed to map BAR 0: %" PRId32 "\n",
            pci->bus.node->name, ret);
        virtio_device_destroy(&device->virtio);
        return ret;
    }

    /* Search for a driver. */
    virtio_match_device(&device->virtio);

    return ret;
}

static pci_match_t virtio_pci_matches[] = {
    { PCI_MATCH_DEVICE(0x1af4, PCI_MATCH_ANY_ID) },
};

static pci_driver_t virtio_pci_driver = {
    .matches = PCI_MATCH_TABLE(virtio_pci_matches),
    .init_device = virtio_pci_init_device,
};

MODULE_NAME("virtio_pci");
MODULE_DESC("VirtIO PCI transport driver");
MODULE_DEPS(PCI_MODULE_NAME, VIRTIO_MODULE_NAME);
MODULE_PCI_DRIVER(virtio_pci_driver);
