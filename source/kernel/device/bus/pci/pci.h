/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               PCI bus manager internal definitions.
 */

#pragma once

#include <device/bus/pci.h>

/* Domain:Bus:Device.Function = 0000:00:00.0 */
#define PCI_NAME_MAX 13

// TODO: Replace with a system that allows runtime selection of the access
// mechanism, will be needed to support memory mapped config access on x86 and
// for ARM64 platforms where the specific implementation is not known until
// runtime.
extern uint8_t platform_pci_config_read8(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write8(pci_address_t *addr, uint8_t reg, uint8_t val);
extern uint16_t platform_pci_config_read16(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write16(pci_address_t *addr, uint8_t reg, uint16_t val);
extern uint32_t platform_pci_config_read32(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write32(pci_address_t *addr, uint8_t reg, uint32_t val);

extern status_t platform_pci_init(void);
extern void platform_pci_unload(void);

extern void pci_scan_bus(uint16_t domain, uint8_t bus);
