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
 * @brief               PCI bus manager internal definitions.
 */

#pragma once

#include <device/bus/pci.h>

/* Domain:Bus:Device.Function = 0000:00:00.0 */
#define PCI_NAME_MAX 13

extern uint8_t platform_pci_config_read8(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write8(pci_address_t *addr, uint8_t reg, uint8_t val);
extern uint16_t platform_pci_config_read16(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write16(pci_address_t *addr, uint8_t reg, uint16_t val);
extern uint32_t platform_pci_config_read32(pci_address_t *addr, uint8_t reg);
extern void platform_pci_config_write32(pci_address_t *addr, uint8_t reg, uint32_t val);

extern status_t platform_pci_init(void);
extern void platform_pci_unload(void);

extern void pci_bus_scan(uint16_t domain, uint8_t bus);
