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
 * @brief               PCI bus manager.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <module.h>
#include <status.h>

#include "pci.h"

/** PCI device bus. */
__export bus_t pci_bus;

static SPINLOCK_DEFINE(pci_config_lock);

static void scan_bus(uint16_t domain, uint8_t bus, int indent);

/** Read an 8-bit value from a PCI device's configuration space. */
__export uint8_t pci_config_read8(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint8_t ret = platform_pci_config_read8(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write an 8-bit value to a PCI device's configuration space. */
__export void pci_config_write8(pci_device_t *device, uint8_t reg, uint8_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write8(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

/** Read a 16-bit value from a PCI device's configuration space. */
__export uint16_t pci_config_read16(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint16_t ret = platform_pci_config_read16(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write a 16-bit value to a PCI device's configuration space. */
__export void pci_config_write16(pci_device_t *device, uint8_t reg, uint16_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write16(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

/** Read a 32-bit value from a PCI device's configuration space. */
__export uint32_t pci_config_read32(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint32_t ret = platform_pci_config_read32(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write a 32-bit value to a PCI device's configuration space. */
__export void pci_config_write32(pci_device_t *device, uint8_t reg, uint32_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write32(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

/** Generate a PCI device name.
 * @param addr          Address of device.
 * @param str           Name buffer (PCI_NAME_MAX). */
static void pci_device_name(pci_address_t *addr, char *str) {
    snprintf(
        str, PCI_NAME_MAX, "%04x:%02x:%02x.%x",
        addr->domain, addr->bus, addr->dev, addr->func);
}

static pci_device_t *scan_device(pci_address_t *addr, int indent) {
    /* Check for device presence. */
    spinlock_lock(&pci_config_lock);
    uint16_t vendor_id = platform_pci_config_read16(addr, PCI_CONFIG_VENDOR_ID);
    spinlock_unlock(&pci_config_lock);

    if (vendor_id == 0xffff)
        return NULL;

    pci_device_t *device = kmalloc(sizeof(pci_device_t), MM_KERNEL);

    bus_device_init(&device->bus);

    device->addr = *addr;

    /* Retrieve common configuration information. */
    spinlock_lock(&pci_config_lock);

    device->device_id      = platform_pci_config_read16(addr, PCI_CONFIG_DEVICE_ID);
    device->vendor_id      = vendor_id;
    device->base_class     = platform_pci_config_read8(addr, PCI_CONFIG_BASE_CLASS);
    device->sub_class      = platform_pci_config_read8(addr, PCI_CONFIG_SUB_CLASS);
    device->prog_iface     = platform_pci_config_read8(addr, PCI_CONFIG_PI);
    device->revision       = platform_pci_config_read8(addr, PCI_CONFIG_REVISION);
    device->header_type    = platform_pci_config_read8(addr, PCI_CONFIG_HEADER_TYPE);
    device->interrupt_line = platform_pci_config_read8(addr, PCI_CONFIG_INTERRUPT_LINE);
    device->interrupt_pin  = platform_pci_config_read8(addr, PCI_CONFIG_INTERRUPT_PIN);

    spinlock_unlock(&pci_config_lock);

    char name[PCI_NAME_MAX];
    pci_device_name(addr, name);

    kprintf(
        LOG_NOTICE, "pci: %*sdevice %s ID %04x:%04x class %02x%02x\n",
        indent, "", name, device->vendor_id, device->device_id, device->base_class,
        device->sub_class);

    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS,          DEVICE_ATTR_STRING, { .string = PCI_DEVICE_CLASS_NAME } },
        { PCI_DEVICE_ATTR_VENDOR_ID,  DEVICE_ATTR_UINT16, { .uint16 = device->vendor_id     } },
        { PCI_DEVICE_ATTR_DEVICE_ID,  DEVICE_ATTR_UINT16, { .uint16 = device->device_id     } },
        { PCI_DEVICE_ATTR_BASE_CLASS, DEVICE_ATTR_UINT8,  { .uint8  = device->base_class    } },
        { PCI_DEVICE_ATTR_SUB_CLASS,  DEVICE_ATTR_UINT8,  { .uint8  = device->sub_class     } },
    };

    status_t ret = bus_create_device(&pci_bus, &device->bus, name, NULL, attrs, array_size(attrs));
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "pci: failed to create device %s: %" PRId32, name, ret);
        kfree(device);
        return NULL;
    }

    /* Check for a PCI-to-PCI bridge. */
    if (device->base_class == 0x06 && device->sub_class == 0x04) {
        spinlock_lock(&pci_config_lock);
        uint8_t dest = platform_pci_config_read8(addr, PCI_CONFIG_P2P_SUBORDINATE_BUS);
        spinlock_unlock(&pci_config_lock);

        kprintf(LOG_NOTICE, "pci: %*sPCI-to-PCI bridge to %02x\n", indent + 1, "", dest);
        scan_bus(addr->domain, dest, indent + 1);
    }

    return device;
}

static void scan_bus(uint16_t domain, uint8_t bus, int indent) {
    kprintf(LOG_NOTICE, "pci: %*sscanning bus %04x:%02x\n", indent, "", domain, bus);

    pci_address_t addr;
    addr.domain = domain;
    addr.bus    = bus;

    for (addr.dev = 0; addr.dev < 32; addr.dev++) {
        addr.func = 0;

        pci_device_t *device = scan_device(&addr, indent + 1);

        if (device && device->header_type & 0x80) {
            /* Multifunction device. */
            for (addr.func = 1; addr.func < 8; addr.func++)
                scan_device(&addr, indent + 1);
        }
    }
}

/** Scan for devices on a bus. */
void pci_bus_scan(uint16_t domain, uint8_t bus) {
    scan_bus(domain, bus, 0);
}

/** Match a PCI device to a driver. */
static bool pci_bus_match_device(bus_device_t *_device, bus_driver_t *_driver) {
    pci_device_t *device = container_of(_device, pci_device_t, bus);
    pci_driver_t *driver = container_of(_driver, pci_driver_t, bus);

    for (size_t i = 0; i < driver->matches.count; i++) {
        pci_match_t *match = &driver->matches.array[i];

        bool matches = true;

        if (match->vendor_id != PCI_MATCH_ANY_ID)
            matches &= device->vendor_id == match->vendor_id;

        if (match->device_id != PCI_MATCH_ANY_ID)
            matches &= device->device_id == match->device_id;

        if (match->base_class != PCI_MATCH_ANY_ID)
            matches &= device->base_class == match->base_class;

        if (match->sub_class != PCI_MATCH_ANY_ID)
            matches &= device->sub_class == match->sub_class;

        if (matches) {
            device->match = match;
            return true;
        }
    }

    return false;
}

/** Initialize a PCI device. */
static status_t pci_bus_init_device(bus_device_t *_device, bus_driver_t *_driver) {
    pci_device_t *device = container_of(_device, pci_device_t, bus);
    pci_driver_t *driver = container_of(_driver, pci_driver_t, bus);

    return driver->init_device(device);
}

static bus_type_t pci_bus_type = {
    .name         = "pci",
    .device_class = PCI_DEVICE_CLASS_NAME,
    .match_device = pci_bus_match_device,
    .init_device  = pci_bus_init_device,
};

static status_t pci_init(void) {
    status_t ret;

    ret = bus_init(&pci_bus, &pci_bus_type);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = platform_pci_init();
    if (ret != STATUS_SUCCESS) {
        bus_destroy(&pci_bus);
        return ret;
    }

    return STATUS_SUCCESS;
}

static status_t pci_unload(void) {
    return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME(PCI_MODULE_NAME);
MODULE_DESC("PCI bus manager");
MODULE_FUNCS(pci_init, pci_unload);
