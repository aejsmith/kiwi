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
#include <device/io.h>

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
     * @param device        Device to initialize.
     * @return              Status code describing the result of the operation. */
    status_t (*init_device)(struct pci_device *device);
} pci_driver_t;

DEFINE_CLASS_CAST(pci_driver, bus_driver, bus);

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

#define PCI_MAX_BARS 6

/** PCI BAR details. */
typedef struct pci_bar {
    phys_ptr_t base;
    phys_size_t size;
    bool is_pio : 1;
    bool prefetchable : 1;
} pci_bar_t;

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

    pci_bar_t bars[PCI_MAX_BARS];       /**< Saved details of BARs. */
} pci_device_t;

DEFINE_CLASS_CAST(pci_device, bus_device, bus);

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

/** Bits in the PCI command register. */
#define PCI_COMMAND_IO                  (1<<0)  /**< I/O Space enable. */
#define PCI_COMMAND_MEMORY              (1<<1)  /**< Memory Space enable. */
#define PCI_COMMAND_BUS_MASTER          (1<<2)  /**< Bus Mastering enable. */
#define PCI_COMMAND_SPECIAL             (1<<3)  /**< Special Cycles enable. */
#define PCI_COMMAND_MWI                 (1<<4)  /**< Memory Write & Invalidate enable. */
#define PCI_COMMAND_VGA_SNOOP           (1<<5)  /**< VGA Pallette Snoop enable. */
#define PCI_COMMAND_PARITY              (1<<6)  /**< Parity Check enable. */
#define PCI_COMMAND_STEPPING            (1<<7)  /**< Stepping enable. */
#define PCI_COMMAND_SERR                (1<<8)  /**< SERR enable. */
#define PCI_COMMAND_FASTB2B             (1<<9)  /**< Fast Back-to-Back enable. */
#define PCI_COMMAND_INT_DISABLE         (1<<10) /**< I/O interrupt disable. */

extern uint8_t pci_config_read8(pci_device_t *device, uint8_t reg);
extern void pci_config_write8(pci_device_t *device, uint8_t reg, uint8_t val);
extern uint16_t pci_config_read16(pci_device_t *device, uint8_t reg);
extern void pci_config_write16(pci_device_t *device, uint8_t reg, uint16_t val);
extern uint32_t config_read32(pci_device_t *device, uint8_t reg);
extern void pci_config_write32(pci_device_t *device, uint8_t reg, uint32_t val);

extern status_t pci_bar_map(pci_device_t *device, uint8_t index, unsigned mmflag, io_region_t *_region);
extern status_t pci_bar_map_etc(
    pci_device_t *device, uint8_t index, phys_ptr_t offset, phys_size_t size,
    uint32_t flags, unsigned mmflag, io_region_t *_region);
extern void pci_bar_unmap(pci_device_t *device, uint8_t index, io_region_t region);
extern void pci_bar_unmap_etc(
    pci_device_t *device, uint8_t index, io_region_t region, phys_ptr_t offset,
    phys_size_t size);

extern status_t device_pci_bar_map(
    device_t *owner, pci_device_t *device, uint8_t index, unsigned mmflag,
    io_region_t *_region);
extern status_t device_pci_bar_map_etc(
    device_t *owner, pci_device_t *device, uint8_t index, phys_ptr_t offset,
    phys_size_t size, uint32_t flags, unsigned mmflag, io_region_t *_region);

extern void pci_enable_master(pci_device_t *device, bool enable);
