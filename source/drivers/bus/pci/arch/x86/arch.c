/*
 * Copyright (C) 2009 Alex Smith
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

#include <drivers/pci.h>

#include <sync/spinlock.h>

#include <endian.h>
#include <errors.h>
#include <module.h>

extern int pci_arch_init(void);

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
 *
 * Reads an 8-bit value from the PCI configuration space at the specified
 * location.
 *
 * @param bus		Bus number to read from.
 * @param dev		Device number to read from.
 * @param func		Function number.
 * @param reg		Register to read.
 *
 * @return		Value read.
 */
uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint8_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in8(PCI_CONFIG_DATA + (reg & 3));
	spinlock_unlock(&pci_config_lock);

	return ret;
}
MODULE_EXPORT(pci_config_read8);

/** Read a 16-bit value from the PCI configuration space.
 *
 * Reads a 16-bit value from the PCI configuration space at the specified
 * location.
 *
 * @param bus		Bus number to read from.
 * @param dev		Device number to read from.
 * @param func		Function number.
 * @param reg		Register to read.
 *
 * @return		Value read (converted to correct endianness).
 */
uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint16_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in16(PCI_CONFIG_DATA + (reg & 2));
	spinlock_unlock(&pci_config_lock);

	return le16_to_cpu(ret);
}
MODULE_EXPORT(pci_config_read16);

/** Read a 32-bit value from the PCI configuration space.
 *
 * Reads a 32-bit value from the PCI configuration space at the specified
 * location.
 *
 * @param bus		Bus number to read from.
 * @param dev		Device number to read from.
 * @param func		Function number.
 * @param reg		Register to read.
 *
 * @return		Value read (converted to correct endianness).
 */
uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
	uint32_t ret;

	spinlock_lock(&pci_config_lock);
	out32(PCI_CONFIG_ADDRESS, PCI_ADDRESS(bus, dev, func, reg));
	ret = in32(PCI_CONFIG_DATA);
	spinlock_unlock(&pci_config_lock);

	return le32_to_cpu(ret);
}
MODULE_EXPORT(pci_config_read32);

/** Check for PCI presence.
 * @return		True if PCI is OK, false if not. */
int pci_arch_init(void) {
	out32(PCI_CONFIG_ADDRESS, 0x80000000);
	return (in32(PCI_CONFIG_ADDRESS) != 0x80000000) ? -ERR_NOT_SUPPORTED : 0;
}
