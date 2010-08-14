/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86-specific PCI functions.
 *
 * Configuration Address Register:
 * ------------------------------------------------------------------
 * | 31 | 30 - 24  | 23 - 16 | 15 - 11 | 10 - 8   | 7 - 2   | 1 - 0 |
 * |----------------------------------------------------------------|
 * | EB | Reserved | Bus No. | Dev No. | Func No. | Reg No. | 00    |
 * ------------------------------------------------------------------
 */

#include <arch/io.h>

#include <sync/spinlock.h>

#include <module.h>
#include <status.h>

#include "../../pci_priv.h"

/** PCI configuration registers. */
#define PCI_CONFIG_ADDRESS	0xCF8	/**< Configuration Address Register. */
#define PCI_CONFIG_DATA		0xCFC	/**< Configuration Data Register. */

/** Macro to generate a a CONFIG_ADDRESS value. */
#define PCI_ADDRESS(bus, dev, func, reg)	\
	((uint32_t)(((bus & 0xFF) << 16) | ((dev & 0x1F) << 11) |	\
	            ((func & 0x07) << 8) | (reg & 0xFC) |		\
	            ((uint32_t)0x80000000)))

/** Lock to protect configuration space. */
static SPINLOCK_DECLARE(pci_config_lock);

/** Read an 8-bit value from the PCI configuration space.
 * @param bus		Bus number to read from.
 * @param dev		Device number to read from.
 * @param func		Function number.
 * @param reg		Register to read.
 * @return		Value read. */
uint8_t pci_arch_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint8_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in8(PCI_CONFIG_DATA + (reg & 3));
	spinlock_unlock(&pci_config_lock);

	return ret;
}

/** Write an 8-bit value to the PCI configuration space.
 * @param bus		Bus number to write to.
 * @param dev		Device number to write to.
 * @param func		Function number.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_arch_config_write8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint8_t val) {
	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	out8(PCI_CONFIG_DATA + (reg & 3), val);
	spinlock_unlock(&pci_config_lock);
}

/** Read a 16-bit value from the PCI configuration space.
 * @return		Value read. */
uint16_t pci_arch_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint16_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in16(PCI_CONFIG_DATA + (reg & 2));
	spinlock_unlock(&pci_config_lock);
	return ret;
}

/** Write a 16-bit value to the PCI configuration space.
 * @param bus		Bus number to write to.
 * @param dev		Device number to write to.
 * @param func		Function number.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_arch_config_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint16_t val) {
	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	out16(PCI_CONFIG_DATA + (reg & 2), val);
	spinlock_unlock(&pci_config_lock);
}

/** Read a 32-bit value from the PCI configuration space.
 * @param bus		Bus number to read from.
 * @param dev		Device number to read from.
 * @param func		Function number.
 * @param reg		Register to read.
 * @return		Value read. */
uint32_t pci_arch_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint32_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in32(PCI_CONFIG_DATA);
	spinlock_unlock(&pci_config_lock);
	return ret;
}

/** Write a 32-bit value to the PCI configuration space.
 * @param bus		Bus number to write to.
 * @param dev		Device number to write to.
 * @param func		Function number.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_arch_config_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) {
	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	out32(PCI_CONFIG_DATA, val);
	spinlock_unlock(&pci_config_lock);
}

/** Check for PCI presence.
 * @return		STATUS_SUCCESS if present, other code if not. */
status_t pci_arch_init(void) {
	out32(PCI_CONFIG_ADDRESS, 0x80000000);
	return (in32(PCI_CONFIG_ADDRESS) != 0x80000000) ? STATUS_NOT_SUPPORTED : STATUS_SUCCESS;
}
