/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		PCI bus manager internal functions.
 */

#ifndef __PCI_PRIV_H
#define __PCI_PRIV_H

#include <drivers/pci.h>

extern uint8_t pci_arch_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
extern void pci_arch_config_write8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint8_t val);
extern uint16_t pci_arch_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
extern void pci_arch_config_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint16_t val);
extern uint32_t pci_arch_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
extern void pci_arch_config_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val);

extern status_t pci_arch_init(void);

#endif /* __PCI_PRIV_H */
