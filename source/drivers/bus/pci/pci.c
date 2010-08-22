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
 *
 * Reference:
 * - Intel 440BX AGPset: 82443BX Host Bridge/Controller
 *   http://www.osdever.net/downloads/docs/29063301.zip
 * - PCI Local Bus specification
 *   http://www.osdever.net/downloads/docs/PCI22.zip
 * - Linux PCI docs
 *   http://www.tldp.org/LDP/tlk/dd/pci.html
 * - OSDev.org Wiki
 *   http://wiki.osdev.org/PCI
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <io/device.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>

#include "pci_priv.h"

static status_t pci_bus_scan(int id, int indent);

/** List of registered PCI drivers. */
static LIST_DECLARE(pci_drivers);
static MUTEX_DECLARE(pci_drivers_lock, 0);

/** PCI bus directory. */
static device_t *pci_bus_dir;

/** Scan a device on a bus.
 * @param bus		Device of bus to scan on.
 * @param id		Bus ID.
 * @param dev		Device number to scan.
 * @param func		Function number to scan.
 * @param indent	Output indentation level.
 * @return		Status code describing result of the operation. */
static status_t pci_device_scan(device_t *bus, int id, int dev, int func, int indent) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "pci-device" } },
		{ "pci.vendor-id", DEVICE_ATTR_UINT16, { .uint16 = 0 } },
		{ "pci.device-id", DEVICE_ATTR_UINT16, { .uint16 = 0 } },
		{ "pci.base-class", DEVICE_ATTR_UINT8, { .uint8 = 0 } },
		{ "pci.sub-class", DEVICE_ATTR_UINT8, { .uint8 = 0 } },
		{ "pci.interface", DEVICE_ATTR_UINT8, { .uint8 = 0 } },
		{ "pci.revision", DEVICE_ATTR_UINT8, { .uint8 = 0 } },
	};
	char name[DEVICE_NAME_MAX];
	pci_device_t *device;
	status_t ret;
	uint8_t dest;

	/* Check vendor ID to determine if device exists. */
	if(pci_arch_config_read16(id, dev, func, PCI_CONFIG_VENDOR_ID) == 0xFFFF) {
		return STATUS_SUCCESS;
	}

	/* Create the device information structure. */
	device = kmalloc(sizeof(*device), MM_SLEEP);
	list_init(&device->header);
	device->driver = NULL;
	device->bus = id;
	device->device = dev;
	device->function = func;

	/* Retrieve device information and fill out attributes. */
	device->vendor_id = attr[1].value.uint16 = pci_config_read16(device, PCI_CONFIG_VENDOR_ID);
	device->device_id = attr[2].value.uint16 = pci_config_read16(device, PCI_CONFIG_DEVICE_ID);
	device->base_class = attr[3].value.uint8 = pci_config_read8(device, PCI_CONFIG_BASE_CLASS);
	device->sub_class = attr[4].value.uint8 = pci_config_read8(device, PCI_CONFIG_SUB_CLASS);
	device->prog_iface = attr[5].value.uint8 = pci_config_read8(device, PCI_CONFIG_PI);
	device->revision = attr[6].value.uint8 = pci_config_read8(device, PCI_CONFIG_REVISION);
	device->cache_line_size = pci_config_read8(device, PCI_CONFIG_CACHE_LINE_SIZE);
	device->header_type = pci_config_read8(device, PCI_CONFIG_HEADER_TYPE);
	device->subsys_vendor = pci_config_read16(device, PCI_CONFIG_SUBSYS_VENDOR);
	device->subsys_id = pci_config_read16(device, PCI_CONFIG_SUBSYS_ID);
	device->interrupt_line = pci_config_read8(device, PCI_CONFIG_INTERRUPT_LINE);
	device->interrupt_pin = pci_config_read8(device, PCI_CONFIG_INTERRUPT_PIN);

	/* Create a device tree node for it. */
	sprintf(name, "%02x.%d", dev, func);
	ret = device_create(name, bus, NULL, device, attr, ARRAYSZ(attr), &device->node);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	kprintf(LOG_NORMAL, "pci: %*sdevice %d:%02x.%d (vendor: 0x%04x, device: 0x%04x, class: 0x%02x 0x%02x)\n",
	        indent, "", id, dev, func, device->vendor_id, device->device_id,
	        device->base_class, device->sub_class);

	/* Check for a PCI-to-PCI bridge. */
	if(device->base_class == 0x06 && device->sub_class == 0x04) {
		dest = pci_config_read8(device, 0x19);
		kprintf(LOG_NORMAL, "pci: %*sdevice %d:%02x.%d is a PCI-to-PCI bridge to %u\n",
		        indent + 1, "", id, dev, func, dest);
		pci_bus_scan(dest, indent + 1);
	}
	return STATUS_SUCCESS;
}

/** Scan a PCI bus for devices.
 * @param id		Bus number to scan.
 * @param indent	Output indentation level.
 * @return		Status code describing result of the operation. */
static status_t pci_bus_scan(int id, int indent) {
	device_attr_t attr = { "type", DEVICE_ATTR_STRING, { .string = "pci-bus" } };
	char name[DEVICE_NAME_MAX];
	device_t *device;
	status_t ret;
	int i, j;

	sprintf(name, "%d", id);
	ret = device_create(name, pci_bus_dir, NULL, NULL, &attr, 1, &device);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	kprintf(LOG_NORMAL, "pci: %*sscanning bus %d for devices...\n", indent, "", id);
	for(i = 0; i < 32; i++) {
		if(pci_arch_config_read8(id, i, 0, PCI_CONFIG_HEADER_TYPE) & 0x80) {
			/* Multifunction device. */
			for(j = 0; j < 8; j++) {
				ret = pci_device_scan(device, id, i, j, indent + 1);
				if(ret != STATUS_SUCCESS) {
					kprintf(LOG_WARN, "pci: warning: failed to scan device %d:%x.%d (%d)\n",
						id, i, j, ret);
				}
			}
		} else {
			ret = pci_device_scan(device, id, i, 0, indent + 1);
			if(ret != STATUS_SUCCESS) {
				kprintf(LOG_WARN, "pci: warning: failed to scan device %d:%x (%d)\n",
					id, i, ret);
			}
		}
	}

	return STATUS_SUCCESS;
}

/** Read an 8-bit value from a device's configuration space.
 * @param device	Device to read from.
 * @param reg		Register to read.
 * @return		Value read. */
uint8_t pci_config_read8(pci_device_t *device, uint8_t reg) {
	return pci_arch_config_read8(device->bus, device->device, device->function, reg);
}
MODULE_EXPORT(pci_config_read8);

/** Write an 8-bit value to a device's configuration space.
 * @param device	Device to write to.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_config_write8(pci_device_t *device, uint8_t reg, uint8_t val) {
	pci_arch_config_write8(device->bus, device->device, device->function, reg, val);
}
MODULE_EXPORT(pci_config_write8);

/** Read a 16-bit value from a device's configuration space.
 * @param device	Device to read from.
 * @param reg		Register to read.
 * @return		Value read. */
uint16_t pci_config_read16(pci_device_t *device, uint8_t reg) {
	return pci_arch_config_read16(device->bus, device->device, device->function, reg);
}
MODULE_EXPORT(pci_config_read16);

/** Write a 16-bit value to a device's configuration space.
 * @param device	Device to write to.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_config_write16(pci_device_t *device, uint8_t reg, uint16_t val) {
	pci_arch_config_write16(device->bus, device->device, device->function, reg, val);
}
MODULE_EXPORT(pci_config_write16);

/** Read a 32-bit value from a device's configuration space.
 * @param device	Device to read from.
 * @param reg		Register to read.
 * @return		Value read. */
uint32_t pci_config_read32(pci_device_t *device, uint8_t reg) {
	return pci_arch_config_read32(device->bus, device->device, device->function, reg);
}
MODULE_EXPORT(pci_config_read32);

/** Write a 32-bit value to a device's configuration space.
 * @param device	Device to write to.
 * @param reg		Register to write.
 * @param val		Value to write. */
void pci_config_write32(pci_device_t *device, uint8_t reg, uint32_t val) {
	pci_arch_config_write32(device->bus, device->device, device->function, reg, val);
}
MODULE_EXPORT(pci_config_write32);

/** Device tree iteration callback for driver addition.
 * @param _device	Device iteration is currently at.
 * @param _driver	Pointer to driver structure.
 * @return		0 if should finish iteration, 1 if should visit
 *			children, 2 if should return to parent. */
static int pci_driver_probe(device_t *_device, void *_driver) {
	pci_device_t *device = _device->data;
	pci_driver_t *driver = _driver;
	device_attr_t *attr;
	size_t i;

	if(_device == pci_bus_dir) {
		return 1;
	}

	attr = device_attr(_device, "type", DEVICE_ATTR_STRING);
	if(!attr) {
		/* We don't visit device children so this won't be triggered
		 * by other drivers not putting a type attribute on. */
		fatal("Missing type attribute in PCI tree (%p)", _device);
	} else if(strcmp(attr->value.string, "pci-bus") == 0) {
		/* For buses, just visit children. */
		return 1;
	} else if(strcmp(attr->value.string, "pci-device") != 0) {
		/* Shouldn't happen, we don't visit children of pci-device's. */
		fatal("Non-PCI device found (%p)", _device);
	}

	/* If the device is already claimed, ignore it. */
	if(device->driver) {
		return 2;
	}

	/* Check if the device matches any entries in the driver's ID table. */
	for(i = 0; i < driver->count; i++) {
		if(driver->ids[i].vendor != PCI_ANY_ID && driver->ids[i].vendor != device->vendor_id) {
			continue;
		} else if(driver->ids[i].device != PCI_ANY_ID && driver->ids[i].device != device->device_id) {
			continue;
		} else if(driver->ids[i].base_class != PCI_ANY_ID && driver->ids[i].base_class != device->base_class) {
			continue;
		} else if(driver->ids[i].sub_class != PCI_ANY_ID && driver->ids[i].sub_class != device->sub_class) {
			continue;
		} else if(driver->ids[i].prog_iface != PCI_ANY_ID && driver->ids[i].prog_iface != device->prog_iface) {
			continue;
		}

		/* We have a match! Call the driver's add device callback. */
		if(!driver->add_device(device, driver->ids[i].data)) {
			continue;
		}

		/* The driver claimed the device, attach it to the driver. */
		list_append(&driver->devices, &device->header);
		device->driver = driver;
	}

	return 2;
}

/** Register a new PCI driver.
 *
 * Registers a new PCI device driver. The driver's add device callback will be
 * called for any PCI devices currently in the system that match the driver.
 *
 * @param driver	Driver to register.
 *
 * @return		Status code describing result of the operation. Failure
 *			can only occur if the structure provided is invalid, in
 *			which cause STATUS_INVALID_ARG will be returned.
 */
status_t pci_driver_register(pci_driver_t *driver) {
	if(!driver || !driver->ids || !driver->count || !driver->add_device) {
		return STATUS_INVALID_ARG;
	}

	list_init(&driver->header);
	list_init(&driver->devices);

	mutex_lock(&pci_drivers_lock);
	list_append(&pci_drivers, &driver->header);
	mutex_unlock(&pci_drivers_lock);

	/* Probe for devices supported by the driver. */
	device_iterate(pci_bus_dir, pci_driver_probe, driver);
	return STATUS_SUCCESS;
}
MODULE_EXPORT(pci_driver_register);

/** Unregister a PCI driver.
 *
 * Unregisters a PCI device driver. All devices managed by the driver will be
 * removed.
 *
 * @param driver	Driver to remove.
 */
void pci_driver_unregister(pci_driver_t *driver) {
	fatal("TODO: pci_driver_unregister");
}
MODULE_EXPORT(pci_driver_unregister);

/** Initialisation function for the PCI module.
 * @return		Status code describing result of the operation. */
static status_t pci_init(void) {
	status_t ret;

	/* Get the architecture to detect PCI presence. */
	ret = pci_arch_init();
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_NORMAL, "pci: PCI is not present or not usable (%d)\n", ret);
		return ret;
	}

	/* Create the PCI bus directory. */
	ret = device_create("pci", device_bus_dir, NULL, NULL, NULL, 0, &pci_bus_dir);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Scan the main bus. */
	return pci_bus_scan(0, 0);
}

/** Unload function for the PCI module.
 * @return		Status code describing result of the operation. */
static status_t pci_unload(void) {
	/* The driver list should be empty: when this is called, there should
	 * be no modules depending on us loaded. */
	assert(list_empty(&pci_drivers));
	return device_destroy(pci_bus_dir);
}

MODULE_NAME("pci");
MODULE_DESC("PCI bus manager");
MODULE_FUNCS(pci_init, pci_unload);
