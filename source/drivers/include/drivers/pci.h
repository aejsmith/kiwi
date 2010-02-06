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
 * @brief		PCI bus module.
 */

#ifndef __DRIVERS_PCI_H
#define __DRIVERS_PCI_H

#include <types/list.h>

struct device;

/** PCI Device structure fields. */
#define PCI_DEVICE_VENDOR_ID		0x00	/**< Vendor ID        - 16-bit. */
#define PCI_DEVICE_DEVICE_ID		0x02	/**< Device ID        - 16-bit. */
#define PCI_DEVICE_COMMAND		0x04	/**< Command          - 16-bit. */
#define PCI_DEVICE_STATUS		0x06	/**< Status           - 16-bit. */
#define PCI_DEVICE_REVISION		0x08	/**< Revision ID      - 8-bit. */
#define PCI_DEVICE_PI			0x09	/**< Prog. Interface  - 8-bit.  */
#define PCI_DEVICE_SUB_CLASS		0x0A	/**< Sub-class        - 8-bit.  */
#define PCI_DEVICE_BASE_CLASS		0x0B	/**< Base class       - 8-bit.  */
#define PCI_DEVICE_CACHESZ		0x0C	/**< Cache line size  - 8-bit.  */
#define PCI_DEVICE_LATENCY		0x0D	/**< Latency timer    - 8-bit.  */
#define PCI_DEVICE_HEADER_TYPE		0x0E	/**< Header type      - 8-bit.  */
#define PCI_DEVICE_BIST			0x0F	/**< BIST             - 8-bit.  */
#define PCI_DEVICE_BAR0			0x10	/**< BAR0             - 32-bit. */
#define PCI_DEVICE_BAR1			0x14	/**< BAR1             - 32-bit. */
#define PCI_DEVICE_BAR2			0x18	/**< BAR2             - 32-bit. */
#define PCI_DEVICE_BAR3			0x1C	/**< BAR3             - 32-bit. */
#define PCI_DEVICE_BAR4			0x20	/**< BAR4             - 32-bit. */
#define PCI_DEVICE_BAR5			0x24	/**< BAR5             - 32-bit. */
#define PCI_DEVICE_CARDBUS_CIS		0x28	/**< Cardbus CIS Ptr  - 32-bit. */
#define PCI_DEVICE_SUBSYS_VENDOR	0x2C	/**< Subsystem vendor - 16-bit. */
#define PCI_DEVICE_SUBSYS_ID		0x2E	/**< Subsystem ID     - 16-bit. */
#define PCI_DEVICE_ROM_ADDR		0x30	/**< ROM base address - 32-bit. */
#define PCI_DEVICE_INTERRUPT_LINE	0x3C	/**< Interrupt line   - 8-bit.  */
#define PCI_DEVICE_INTERRUPT_PIN	0x3D	/**< Interrupt pin    - 8-bit.  */
#define PCI_DEVICE_MIN_GRANT		0x3E	/**< Min grant        - 8-bit.  */
#define PCI_DEVICE_MAX_LATENCY		0x3F	/**< Max latency      - 8-bit.  */

/** Value to match any ID in the structure below. */
#define PCI_ANY_ID			(~((uint32_t)0))

/** Structure describing PCI device IDs to look up. */
typedef struct pci_device_id {
	uint32_t vendor;			/**< Vendor ID. */
	uint32_t device;			/**< Device ID. */
	uint32_t base_class;			/**< Base class. */
	uint32_t sub_class;			/**< Sub class. */
	void *data;				/**< Driver data. */
} pci_device_id_t;

/** PCI device lookup callback function.
 * @param dev		Device that matched.
 * @param id		ID structure the device matches.
 * @return		Whether to continue lookup. */
typedef bool (*pci_lookup_t)(struct device *dev, pci_device_id_t *id);

extern uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
extern uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);
extern uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg);

extern uint8_t pci_device_read8(struct device *device, uint8_t reg);
extern uint16_t pci_device_read16(struct device *device, uint8_t reg);
extern uint32_t pci_device_read32(struct device *device, uint8_t reg);

extern bool pci_device_lookup(pci_device_id_t *ids, size_t count, pci_lookup_t cb);

#endif /* __DRIVERS_PCI_H */
