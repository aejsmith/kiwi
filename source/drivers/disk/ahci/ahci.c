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
 * @brief		AHCI controller driver.
 */

#include <lib/utility.h>

#include <module.h>
#include <status.h>

#include "ahci.h"

/** AHCI device ID table. */
static pci_device_id_t ahci_device_ids[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x01, 0x06, 0x01, NULL },
};

/** ATA PCI driver structure. */
static pci_driver_t ahci_driver = {
	.ids = ahci_device_ids,
	.count = ARRAYSZ(ahci_device_ids),
	.add_device = ahci_hba_add,
};

/** Initialisation function for the AHCI driver.
 * @return		Status code describing result of the operation. */
static status_t ahci_init(void) {
	return pci_driver_register(&ahci_driver);
}

/** Unloading function for the AHCI driver.
 * @return		Status code describing result of the operation. */
static status_t ahci_unload(void) {
	pci_driver_unregister(&ahci_driver);
	return STATUS_SUCCESS;
}

MODULE_NAME("ahci");
MODULE_DESC("AHCI controller driver");
MODULE_FUNCS(ahci_init, ahci_unload);
MODULE_DEPS("ata", "pci");
