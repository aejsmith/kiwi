/* Kiwi PCI bus module
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

#include <console/kprintf.h>

#include <drivers/pci.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <io/device.h>

#include <errors.h>
#include <module.h>

/** Structure to store information about a PCI device. */
typedef struct pci_device {
	uint8_t bus;			/**< Bus ID. */
	uint8_t dev;			/**< Device number. */
	uint8_t func;			/**< Function number. */
} pci_device_t;

extern int pci_arch_init(void);
static int pci_bus_scan(int id);

/** PCI bus directory. */
static device_t *pci_bus_dir;

/** Scan a device on a bus.
 * @param bus		Device of bus to scan on.
 * @param id		Bus ID.
 * @param dev		Device number to scan.
 * @param func		Function number to scan.
 * @return		0 on success, negative error code on failure. */
static int pci_device_scan(device_t *bus, int id, int dev, int func) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { string: "pci-device" } },
		{ "pci.vendor-id", DEVICE_ATTR_UINT16,
			{ uint16: pci_config_read16(id, dev, func, PCI_DEVICE_VENDOR_ID) },
		},
		{ "pci.device-id", DEVICE_ATTR_UINT16,
			{ uint16: pci_config_read16(id, dev, func, PCI_DEVICE_DEVICE_ID) },
		},
		{ "pci.revision", DEVICE_ATTR_UINT8,
			{ uint8: pci_config_read8(id, dev, func, PCI_DEVICE_REVISION) },
		},
		{ "pci.interface", DEVICE_ATTR_UINT8,
			{ uint8: pci_config_read8(id, dev, func, PCI_DEVICE_PI) },
		},
		{ "pci.base-class", DEVICE_ATTR_UINT8,
			{ uint8: pci_config_read8(id, dev, func, PCI_DEVICE_BASE_CLASS) },
		},
		{ "pci.sub-class", DEVICE_ATTR_UINT8,
			{ uint8: pci_config_read8(id, dev, func, PCI_DEVICE_SUB_CLASS) },
		},
	};
	char name[DEVICE_NAME_MAX];
	pci_device_t *info;
	device_t *device;
	uint8_t dest;
	int ret;

	/* Check vendor ID to determine if device exists. */
	if(attr[1].value.uint16 == 0xFFFF) {
		return 0;
	}

	/* Create a structure to store bus/device/function numbers, so we don't
	 * have to keep parsing the device names when operating on devices. */
	info = kmalloc(sizeof(pci_device_t), MM_SLEEP);
	info->bus = id;
	info->dev = dev;
	info->func = func;

	/* Create a device tree node for it. */
	sprintf(name, "%02x.%d", dev, func);
	if((ret = device_create(name, bus, NULL, info, attr, ARRAYSZ(attr), &device)) != 0) {
		return ret;
	}

	kprintf(LOG_DEBUG, "pci: got device %d:%02x.%d (vendor: 0x%04x, device: 0x%04x, class: 0x%02x 0x%02x)\n",
	        id, dev, func, attr[1].value.uint16, attr[2].value.uint16,
	        attr[5].value.uint8, attr[6].value.uint8);

	/* Check for a PCI-to-PCI bridge. */
	if(attr[5].value.uint8 == 0x06 && attr[6].value.uint8 == 0x04) {
		dest = pci_config_read8(id, dev, func, 0x19);
		kprintf(LOG_DEBUG, "pci: device %d:%02x.%d is a PCI-to-PCI bridge to %u\n",
		        id, dev, func, dest);
		pci_bus_scan(dest);
	}
	return 0;
}

/** Scan a PCI bus for devices.
 * @param id		Bus number to scan.
 * @return		0 on success, negative error code on failure. */
static int pci_bus_scan(int id) {
	device_attr_t attr = { "type", DEVICE_ATTR_STRING, { string: "pci-bus" } };
	char name[DEVICE_NAME_MAX];
	device_t *device;
	int i, j, ret;

	sprintf(name, "%d", id);
	if((ret = device_create(name, pci_bus_dir, NULL, NULL, &attr, 1, &device)) != 0) {
		return ret;
	}

	kprintf(LOG_DEBUG, "pci: scanning bus %p(%d) for devices...\n", device, id);
	for(i = 0; i < 32; i++) {
		if(pci_config_read8(id, i, 0, PCI_DEVICE_HEADER_TYPE) & 0x80) {
			for(j = 0; j < 8; j++) {
				if((ret = pci_device_scan(device, id, i, j)) != 0) {
					kprintf(LOG_WARN, "pci: warning: failed to scan device %d:%x.%d: %d\n",
						id, i, j, ret);
				}
			}
		} else {
			if((ret = pci_device_scan(device, id, i, 0)) != 0) {
				kprintf(LOG_WARN, "pci: warning: failed to scan device %d:%x: %d\n",
					id, i, ret);
			}
		}
	}

	return 0;
}

/** Read an 8-bit value from a device's configuration space.
 *
 * Reads an 8-bit value from the PCI configuration space for a certain device.
 *
 * @param device	Device to read from.
 * @param reg		Register to read.
 *
 * @return		Value read, or 0 if device was not a PCI device.
 */
uint8_t pci_device_read8(device_t *device, uint8_t reg) {
	pci_device_t *info = device->data;
	device_attr_t *attr;

	if(!(attr = device_attr(device, "type", DEVICE_ATTR_STRING)) || strcmp(attr->value.string, "pci-device")) {
		return 0;
	}

	return pci_config_read8(info->bus, info->dev, info->func, reg);
}
MODULE_EXPORT(pci_device_read8);

/** Read a 16-bit value from a device's configuration space.
 *
 * Reads a 16-bit value from the PCI configuration space for a certain device.
 *
 * @param device	Device to read from.
 * @param reg		Register to read.
 *
 * @return		Value read (converted to correct endianness), or 0 if
 *			device was not a PCI device.
 */
uint16_t pci_device_read16(device_t *device, uint8_t reg) {
	pci_device_t *info = device->data;
	device_attr_t *attr;

	if(!(attr = device_attr(device, "type", DEVICE_ATTR_STRING)) || strcmp(attr->value.string, "pci-device")) {
		return 0;
	}

	return pci_config_read16(info->bus, info->dev, info->func, reg);
}
MODULE_EXPORT(pci_device_read16);

/** Read a 32-bit value from a device's configuration space.
 *
 * Reads a 32-bit value from the PCI configuration space for a certain device.
 *
 * @param device	Device to read from.
 * @param reg		Register to read.
 *
 * @return		Value read (converted to correct endianness), or 0 if
 *			device was not a PCI device.
 */
uint32_t pci_device_read32(device_t *device, uint8_t reg) {
	pci_device_t *info = device->data;
	device_attr_t *attr;

	if(!(attr = device_attr(device, "type", DEVICE_ATTR_STRING)) || strcmp(attr->value.string, "pci-device")) {
		return 0;
	}

	return pci_config_read32(info->bus, info->dev, info->func, reg);
}
MODULE_EXPORT(pci_device_read32);

/** PCI lookup state structure. */
typedef struct pci_lookup_state {
	pci_device_id_t *ids;		/**< ID structures. */
	size_t count;			/**< Number of ID structures. */
	pci_lookup_t cb;		/**< Callback function. */
	size_t matched;			/**< Number of devices matched. */
} pci_lookup_state_t;

/** Device tree iteration callback for PCI lookup.
 * @param device	Device iteration is currently at.
 * @param data		State structure pointer.
 * @return		0 if should finish iteration, 1 if should visit
 *			children, 2 if should return to parent. */
static int pci_device_lookup_func(device_t *device, void *data) {
	pci_lookup_state_t *state = data;
	device_attr_t *attr;
	uint16_t vid, did;
	uint8_t base, sub;
	size_t i;

	if(device == pci_bus_dir) {
		return 1;
	} else if(!(attr = device_attr(device, "type", DEVICE_ATTR_STRING))) {
		/* We don't visit device children so this won't be triggered
		 * by other drivers not putting a type attribute on. */
		fatal("Missing type attribute in PCI tree (%p)", device);
	} else if(strcmp(attr->value.string, "pci-bus") == 0) {
		/* For buses, just visit children. */
		return 1;
	} else if(strcmp(attr->value.string, "pci-device") != 0) {
		/* Shouldn't happen, we don't visit children of pci-device's. */
		fatal("Non-PCI device found (%p)", device);
	}

	/* Get device information. */
	vid = pci_device_read16(device, PCI_DEVICE_VENDOR_ID);
	did = pci_device_read16(device, PCI_DEVICE_DEVICE_ID);
	base = pci_device_read8(device, PCI_DEVICE_BASE_CLASS);
	sub = pci_device_read8(device, PCI_DEVICE_SUB_CLASS);

	for(i = 0; i < state->count; i++) {
		if(state->ids[i].vendor != PCI_ANY_ID && state->ids[i].vendor != vid) {
			continue;
		} else if(state->ids[i].device != PCI_ANY_ID && state->ids[i].device != did) {
			continue;
		} else if(state->ids[i].base_class != PCI_ANY_ID && state->ids[i].base_class != base) {
			continue;
		} else if(state->ids[i].sub_class != PCI_ANY_ID && state->ids[i].sub_class != sub) {
			continue;
		}

		state->matched = true;
		if(state->cb(device, &state->ids[i])) {
			return 2;
		} else {
			return 0;
		}
	}

	return 2;
}

/** Look up PCI devices.
 *
 * Iterates through the PCI device tree and calls the provided function on any
 * devices that match any of the structures in the provided array.
 *
 * @param ids		Array of ID structures to match against.
 * @param count		Number of entries in array.
 * @param cb		Function to call on matching devices.
 *
 * @return		Whether any devices matched.
 */
bool pci_device_lookup(pci_device_id_t *ids, size_t count, pci_lookup_t cb) {
	pci_lookup_state_t state;

	state.ids = ids;
	state.count = count;
	state.cb = cb;
	state.matched = 0;

	device_iterate(pci_bus_dir, pci_device_lookup_func, &state);
	return (state.matched != 0);
}
MODULE_EXPORT(pci_device_lookup);

/** Initialization function for the PCI module.
 * @return		0 on success, negative error code on failure. */
static int pci_init(void) {
	int ret;

	/* Get the architecture to detect PCI presence. */
	if((ret = pci_arch_init()) != 0) {
		kprintf(LOG_DEBUG, "pci: PCI is not present or not usable (%d)\n", ret);
		return ret;
	}

	/* Create the PCI bus directory. */
	if((ret = device_create("pci", device_bus_dir, NULL, NULL, NULL, 0, &pci_bus_dir)) != 0) {
		return ret;
	}

	/* Scan the main bus. */
	return pci_bus_scan(0);
}

/** Unload function for the PCI module.
 * @return		0 on success, negative error code on failure. */
static int pci_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("pci");
MODULE_DESC("PCI bus manager");
MODULE_FUNCS(pci_init, pci_unload);
