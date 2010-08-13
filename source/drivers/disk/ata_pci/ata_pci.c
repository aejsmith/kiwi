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

#include <module.h>
#include <status.h>

/** Initialisation function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_pci_init(void) {
	return STATUS_SUCCESS;
}

/** Unloading function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_pci_unload(void) {
	return STATUS_SUCCESS;
}

MODULE_NAME("ata_pci");
MODULE_DESC("PCI ATA device driver");
MODULE_FUNCS(ata_pci_init, ata_pci_unload);
MODULE_DEPS("ata", "pci");
