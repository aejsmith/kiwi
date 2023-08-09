/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               PC platform PCI functions.
 */

#include <arch/io.h>

#include <kernel.h>
#include <status.h>

#include "../pci.h"

/** Configuration Space Access Mechanism #1. */
#define PCI_CONFIG_ADDRESS  0xcf8
#define PCI_CONFIG_DATA     0xcfc

static inline uint32_t config_address(pci_address_t *addr, uint8_t reg) {
    uint32_t val = 0x80000000;
    val |= (addr->bus  & 0xff) << 16;
    val |= (addr->dev  & 0x1f) << 11;
    val |= (addr->func & 0x07) << 8;
    val |= (reg        & 0xfc) << 0;
    return val;
}

uint8_t platform_pci_config_read8(pci_address_t *addr, uint8_t reg) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    return in8(PCI_CONFIG_DATA + (reg & 3));
}

void platform_pci_config_write8(pci_address_t *addr, uint8_t reg, uint8_t val) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    out8(PCI_CONFIG_DATA + (reg & 3), val);
}

uint16_t platform_pci_config_read16(pci_address_t *addr, uint8_t reg) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    return in16(PCI_CONFIG_DATA + (reg & 2));
}

void platform_pci_config_write16(pci_address_t *addr, uint8_t reg, uint16_t val) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    out16(PCI_CONFIG_DATA + (reg & 2), val);
}

uint32_t platform_pci_config_read32(pci_address_t *addr, uint8_t reg) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    return in32(PCI_CONFIG_DATA);
}

void platform_pci_config_write32(pci_address_t *addr, uint8_t reg, uint32_t val) {
    out32(PCI_CONFIG_ADDRESS, config_address(addr, reg));
    out32(PCI_CONFIG_DATA, val);
}

status_t platform_pci_init(void) {
    /* TODO: Support the memory mapped configuration mechanism. */

    /* Check for PCI presence. */
    out32(PCI_CONFIG_ADDRESS, 0x80000000);
    if (in32(PCI_CONFIG_ADDRESS) != 0x80000000) {
        kprintf(LOG_NOTICE, "pci: PCI is not present\n");
        return STATUS_NOT_SUPPORTED;
    }

    pci_scan_bus(0, 0);
    return STATUS_SUCCESS;
}

void platform_pci_unload(void) {
    /* Nothing happens. */
}
