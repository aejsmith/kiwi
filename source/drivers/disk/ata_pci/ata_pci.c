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
 * @brief		PCI ATA device driver.
 */

#include <drivers/ata.h>
#include <drivers/pci.h>

#include <lib/utility.h>

#include <console.h>
#include <module.h>
#include <status.h>

/** Add a new PCI ATA device.
 * @param device	Device that was matched.
 * @param data		Unused.
 * @return		Whether the device has been claimed. */
static bool ata_pci_add_device(pci_device_t *device, void *data) {
	kprintf(LOG_NORMAL, "ata: found PCI ATA device %d:%02x.%d (vendor: 0x%04x, id: 0x%04x)\n",
	        device->bus, device->device, device->function, device->vendor_id,
	        device->device_id);
	return true;
}

/** ATA PCI device ID structure. */
static pci_device_id_t ata_pci_ids[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x01, 0x01, NULL },
};

/** ATA PCI driver structure. */
static pci_driver_t ata_pci_driver = {
	.ids = ata_pci_ids,
	.count = ARRAYSZ(ata_pci_ids),
	.add_device = ata_pci_add_device,
};

/** Initialisation function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_pci_init(void) {
	return pci_driver_register(&ata_pci_driver);
}

/** Unloading function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_pci_unload(void) {
	pci_driver_unregister(&ata_pci_driver);
	return STATUS_SUCCESS;
}

MODULE_NAME("ata_pci");
MODULE_DESC("PCI ATA device driver");
MODULE_FUNCS(ata_pci_init, ata_pci_unload);
MODULE_DEPS("ata", "pci");
