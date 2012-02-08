/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	.count = ARRAY_SIZE(ahci_device_ids),
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
