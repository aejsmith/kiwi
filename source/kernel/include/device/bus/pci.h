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

#define PCI_MODULE_NAME "pci"

extern bus_t pci_bus;

/** PCI driver structure. */
typedef struct pci_driver {
    bus_driver_t bus;
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

    pci_address_t addr;

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