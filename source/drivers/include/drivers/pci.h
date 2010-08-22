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
 * @brief		PCI bus manager.
 */

#ifndef __DRIVERS_PCI_H
#define __DRIVERS_PCI_H

#ifndef KERNEL
# error "This header is for kernel/driver use only"
#endif

#include <lib/list.h>

struct device;
struct pci_device;

/** Offsets into PCI configuration space. */
#define PCI_CONFIG_VENDOR_ID		0x00	/**< Vendor ID        - 16-bit. */
#define PCI_CONFIG_DEVICE_ID		0x02	/**< Device ID        - 16-bit. */
#define PCI_CONFIG_COMMAND		0x04	/**< Command          - 16-bit. */
#define PCI_CONFIG_STATUS		0x06	/**< Status           - 16-bit. */
#define PCI_CONFIG_REVISION		0x08	/**< Revision ID      - 8-bit. */
#define PCI_CONFIG_PI			0x09	/**< Prog. Interface  - 8-bit.  */
#define PCI_CONFIG_SUB_CLASS		0x0A	/**< Sub-class        - 8-bit.  */
#define PCI_CONFIG_BASE_CLASS		0x0B	/**< Base class       - 8-bit.  */
#define PCI_CONFIG_CACHE_LINE_SIZE	0x0C	/**< Cache line size  - 8-bit.  */
#define PCI_CONFIG_LATENCY		0x0D	/**< Latency timer    - 8-bit.  */
#define PCI_CONFIG_HEADER_TYPE		0x0E	/**< Header type      - 8-bit.  */
#define PCI_CONFIG_BIST			0x0F	/**< BIST             - 8-bit.  */
#define PCI_CONFIG_BAR0			0x10	/**< BAR0             - 32-bit. */
#define PCI_CONFIG_BAR1			0x14	/**< BAR1             - 32-bit. */
#define PCI_CONFIG_BAR2			0x18	/**< BAR2             - 32-bit. */
#define PCI_CONFIG_BAR3			0x1C	/**< BAR3             - 32-bit. */
#define PCI_CONFIG_BAR4			0x20	/**< BAR4             - 32-bit. */
#define PCI_CONFIG_BAR5			0x24	/**< BAR5             - 32-bit. */
#define PCI_CONFIG_CARDBUS_CIS		0x28	/**< Cardbus CIS Ptr  - 32-bit. */
#define PCI_CONFIG_SUBSYS_VENDOR	0x2C	/**< Subsystem vendor - 16-bit. */
#define PCI_CONFIG_SUBSYS_ID		0x2E	/**< Subsystem ID     - 16-bit. */
#define PCI_CONFIG_ROM_ADDR		0x30	/**< ROM base address - 32-bit. */
#define PCI_CONFIG_INTERRUPT_LINE	0x3C	/**< Interrupt line   - 8-bit.  */
#define PCI_CONFIG_INTERRUPT_PIN	0x3D	/**< Interrupt pin    - 8-bit.  */
#define PCI_CONFIG_MIN_GRANT		0x3E	/**< Min grant        - 8-bit.  */
#define PCI_CONFIG_MAX_LATENCY		0x3F	/**< Max latency      - 8-bit.  */

/** Bits in the PCI command register. */
#define PCI_COMMAND_IO			(1<<0)	/**< I/O Space enable. */
#define PCI_COMMAND_MEMORY		(1<<1)	/**< Memory Space enable. */
#define PCI_COMMAND_BUS_MASTER		(1<<2)	/**< Bus Mastering enable. */
#define PCI_COMMAND_SPECIAL		(1<<3)	/**< Special Cycles enable. */
#define PCI_COMMAND_MWI			(1<<4)	/**< Memory Write & Invalidate enable. */
#define PCI_COMMAND_VGA_SNOOP		(1<<5)	/**< VGA Pallette Snoop enable. */
#define PCI_COMMAND_PARITY		(1<<6)	/**< Parity Check enable. */
#define PCI_COMMAND_STEPPING		(1<<7)	/**< Stepping enable. */
#define PCI_COMMAND_SERR		(1<<8)	/**< SERR enable. */
#define PCI_COMMAND_FASTB2B		(1<<9)	/**< Fast Back-to-Back enable. */
#define PCI_COMMAND_INT_DISABLE		(1<<10)	/**< I/O interrupt disable. */

/** Mask to clear special bits from an I/O address. */
#define PCI_IO_ADDRESS_MASK		0xFFFFFFFC

/** Value to match any ID in the structure below. */
#define PCI_ANY_ID			(~((uint32_t)0))

/** Structure describing PCI device IDs to look up. */
typedef struct pci_device_id {
	uint32_t vendor;			/**< Vendor ID. */
	uint32_t device;			/**< Device ID. */
	uint32_t base_class;			/**< Base class. */
	uint32_t sub_class;			/**< Sub class. */
	uint32_t prog_iface;			/**< Programming interface. */
	void *data;				/**< Driver data. */
} pci_device_id_t;

/** PCI driver information structure. */
typedef struct pci_driver {
	list_t header;				/**< Link to PCI driver list. */
	list_t devices;				/**< Devices claimed by the driver. */

	pci_device_id_t *ids;			/**< Array of devices recognised by the driver. */
	size_t count;				/**< Number of devices in the array. */

	/** Called when a device is matched to the driver.
	 * @param device	Device that was matched.
	 * @param data		Data pointer set for the device ID matched.
	 * @return		Whether the driver has claimed the device. */
	bool (*add_device)(struct pci_device *device, void *data);
} pci_driver_t;

/** PCI device information structure. */
typedef struct pci_device {
	/** Linkage to device tree and driver. */
	list_t header;				/**< Link to driver's devices list. */
	pci_driver_t *driver;			/**< Driver that has claimed the device. */
	struct device *node;			/**< Device tree node for the device. */

	/** Location of the device. */
	uint8_t bus;				/**< Bus ID. */
	uint8_t device;				/**< Device number. */
	uint8_t function;			/**< Function number. */

	/** Information about the device. */
	uint16_t vendor_id;			/**< Vendor ID. */
	uint16_t device_id;			/**< Device ID. */
	uint8_t base_class;			/**< Class ID. */
	uint8_t sub_class;			/**< Sub-class ID. */
	uint8_t prog_iface;			/**< Programming interface. */
	uint8_t revision;			/**< Revision. */
	uint8_t cache_line_size;		/**< Cache line size (number of DWORDs). */
	uint8_t header_type;			/**< Header type. */
	uint16_t subsys_vendor;			/**< Subsystem vendor. */
	uint16_t subsys_id;			/**< Subsystem ID. */
	uint8_t interrupt_line;			/**< Interrupt line. */
	uint8_t interrupt_pin;			/**< Interrupt pin. */
} pci_device_t;

extern uint8_t pci_config_read8(pci_device_t *device, uint8_t reg);
extern void pci_config_write8(pci_device_t *device, uint8_t reg, uint8_t val);
extern uint16_t pci_config_read16(pci_device_t *device, uint8_t reg);
extern void pci_config_write16(pci_device_t *device, uint8_t reg, uint16_t val);
extern uint32_t pci_config_read32(pci_device_t *device, uint8_t reg);
extern void pci_config_write32(pci_device_t *device, uint8_t reg, uint32_t val);

extern status_t pci_driver_register(pci_driver_t *driver);
extern void pci_driver_unregister(pci_driver_t *driver);

#endif /* __DRIVERS_PCI_H */
