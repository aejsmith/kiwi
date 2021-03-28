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

#pragma once

#include <device/bus.h>

#include <kernel/device/bus/pci.h>

#include <lib/utility.h>

struct pci_device;

#define PCI_MODULE_NAME "pci"

extern bus_t pci_bus;

/**
 * PCI match structure. This is used to define the devices that a driver matches
 * against. Fields that a driver does not care about should be set to
 * PCI_MATCH_ANY_ID. Use the helper PCI_MATCH_*() macros as a shorthand to
 * initialize the structure with only relevant fields and set others to
 * PCI_MATCH_ANY_ID.
 */
typedef struct pci_match {
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t base_class;
    uint32_t sub_class;

    /** Pointer to driver-private data (e.g. for device-specific configuration). */
    void* private;
} pci_match_t;

#define PCI_MATCH_ANY_ID    (~0u)

/** Initialize a PCI match entry for vendor/device IDs only. */
#define PCI_MATCH_DEVICE(_vendor_id, _device_id) \
    .vendor_id  = _vendor_id, \
    .device_id  = _device_id, \
    .base_class = PCI_MATCH_ANY_ID, \
    .sub_class  = PCI_MATCH_ANY_ID

/** Initialize a PCI match entry for class IDs only. */
#define PCI_MATCH_CLASS(_base_class, _sub_class) \
    .vendor_id  = PCI_MATCH_ANY_ID, \
    .device_id  = PCI_MATCH_ANY_ID, \
    .base_class = _base_class, \
    .sub_class  = _sub_class

/** PCI match table. */
typedef struct pci_match_table {
    pci_match_t *array;
    size_t count;
} pci_match_table_t;

/**
 * Initialize a PCI match table. This is for use within the definition of the
 * PCI driver. Example definition of a match table:
 *
 *   static pci_match_t my_pci_driver_matches[] = {
 *       { PCI_MATCH_DEVICE(0x1234, 0x5678) },
 *       { PCI_MATCH_DEVICE(0x1234, 0x9abc), &device_9abc_data },
 *   };
 *
 *   static pci_driver_t my_pci_driver = {
 *       .matches = PCI_MATCH_TABLE(my_pci_driver_matches),
 *       ...
 *   };
 */
#define PCI_MATCH_TABLE(table) { table, array_size(table) }

/** PCI driver structure. */
typedef struct pci_driver {
    bus_driver_t bus;

    pci_match_table_t matches;          /**< Devices that the driver supports. */

    /** Initialize a device that matched against this driver.
     * @param device        Device to match.
     * @return              Status code describing the result of the operation. */
    status_t (*init_device)(struct pci_device *device);
} pci_driver_t;

/** Define module init/unload functions for a PCI driver.
 * @param driver        Driver to register. */
#define MODULE_PCI_DRIVER(driver) \
    MODULE_BUS_DRIVER(pci_bus, driver)

/** Address identifying a PCI device's location. */
typedef struct pci_address {
    uint16_t domain;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
} pci_address_t;

/** PCI device structure. */
typedef struct pci_device {
    bus_device_t bus;

    pci_address_t addr;                 /**< Device location. */
    pci_match_t *match;                 /**< Driver match. */

    /** Common configuration header properties. */
    uint16_t device_id;
    uint16_t vendor_id;
    uint8_t base_class;
    uint8_t sub_class;
    uint8_t prog_iface;
    uint8_t revision;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
} pci_device_t;

/** Get the device tree node for a PCI device. */
static inline device_t *pci_device_node(pci_device_t *device) {
    return device->bus.node;
}

/** Common PCI configuration offsets. */
#define PCI_CONFIG_VENDOR_ID            0x00    /**< Vendor ID        (16-bit). */
#define PCI_CONFIG_DEVICE_ID            0x02    /**< Device ID        (16-bit). */
#define PCI_CONFIG_COMMAND              0x04    /**< Command          (16-bit). */
#define PCI_CONFIG_STATUS               0x06    /**< Status           (16-bit). */
#define PCI_CONFIG_REVISION             0x08    /**< Revision ID      (8-bit). */
#define PCI_CONFIG_PI                   0x09    /**< Prog. Interface  (8-bit). */
#define PCI_CONFIG_SUB_CLASS            0x0a    /**< Sub-class        (8-bit). */
#define PCI_CONFIG_BASE_CLASS           0x0b    /**< Base class       (8-bit). */
#define PCI_CONFIG_CACHE_LINE_SIZE      0x0c    /**< Cache line size  (8-bit). */
#define PCI_CONFIG_LATENCY              0x0d    /**< Latency timer    (8-bit). */
#define PCI_CONFIG_HEADER_TYPE          0x0e    /**< Header type      (8-bit). */
#define PCI_CONFIG_BIST                 0x0f    /**< BIST             (8-bit). */

/** General device configuration offsets (header type = 0x00). */
#define PCI_CONFIG_BAR0                 0x10    /**< BAR0             (32-bit). */
#define PCI_CONFIG_BAR1                 0x14    /**< BAR1             (32-bit). */
#define PCI_CONFIG_BAR2                 0x18    /**< BAR2             (32-bit). */
#define PCI_CONFIG_BAR3                 0x1c    /**< BAR3             (32-bit). */
#define PCI_CONFIG_BAR4                 0x20    /**< BAR4             (32-bit). */
#define PCI_CONFIG_BAR5                 0x24    /**< BAR5             (32-bit). */
#define PCI_CONFIG_CARDBUS_CIS          0x28    /**< Cardbus CIS Ptr  (32-bit). */
#define PCI_CONFIG_SUBSYS_VENDOR        0x2c    /**< Subsystem vendor (16-bit). */
#define PCI_CONFIG_SUBSYS_ID            0x2e    /**< Subsystem ID     (16-bit). */
#define PCI_CONFIG_ROM_ADDR             0x30    /**< ROM base address (32-bit). */
#define PCI_CONFIG_INTERRUPT_LINE       0x3c    /**< Interrupt line   (8-bit). */
#define PCI_CONFIG_INTERRUPT_PIN        0x3d    /**< Interrupt pin    (8-bit). */
#define PCI_CONFIG_MIN_GRANT            0x3e    /**< Min grant        (8-bit). */
#define PCI_CONFIG_MAX_LATENCY          0x3f    /**< Max latency      (8-bit). */

/** PCI-to-PCI bridge configuration offsets (header type = 0x01). */
#define PCI_CONFIG_P2P_SUBORDINATE_BUS  0x19    /**< Subordinate bus  (8-bit). */
