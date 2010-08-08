/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Generic ATA device driver.
 *
 * Reference:
 * - PCI IDE Controller Specification
 *   http://suif.stanford.edu/~csapuntz/specs/pciide.ps
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <drivers/pci.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>

#include "ata_priv.h"

/** Check if a controller is in compatibility mode. */
#define ATA_IS_COMPAT(pi)	((pi) == 0x00 || (pi) == 0x02)

/** PCI lookup callback for ATA devices.
 * @param device	Device that matched.
 * @param id		ID structure for match.
 * @return		Whether to continue lookup. */
static bool ata_pci_lookup_func(device_t *device, pci_device_id_t *id) {
	ata_controller_t *controller;
	uint32_t ctl, cmd, irq;
	uint8_t pri_pi, sec_pi;

	kprintf(LOG_NORMAL, "ata: found PCI ATA device %s:%s (id: 0x%04x, vendor: 0x%04x)\n",
		device->parent->name, device->name, pci_device_read16(device, PCI_DEVICE_DEVICE_ID),
		pci_device_read16(device, PCI_DEVICE_VENDOR_ID));

	/* Get the programming interface so we can find out what mode the
	 * controllers are operating in. The primary controller's interface
	 * is in the lower 2 bits, so we clear the higher 2 bits to find it.
	 * The secondary controller's interface, obviously, is in the higher
	 * 2 bits, so we we shift it right by 2 bits. */
	pri_pi = (pci_device_read8(device, PCI_DEVICE_PI) & 0x0F) & ~0x0C;
	sec_pi = (pci_device_read8(device, PCI_DEVICE_PI) & 0x0F) >> 2;

	/* Add primary controller. Compatibility-mode controllers always have
	 * the same details. */
	ctl = ATA_IS_COMPAT(pri_pi) ? 0x3F0 : pci_device_read32(device, PCI_DEVICE_BAR0);
	cmd = ATA_IS_COMPAT(pri_pi) ? 0x1F0 : pci_device_read32(device, PCI_DEVICE_BAR1);
	irq = ATA_IS_COMPAT(pri_pi) ? 14    : pci_device_read8 (device, PCI_DEVICE_INTERRUPT_LINE);
	controller = ata_controller_add(device, ctl, cmd, irq);
	if(controller) {
		kprintf(LOG_NORMAL, " primary:   %" PRId32 " (controller: %p, pi: %s)\n",
			controller->id, controller, ATA_IS_COMPAT(pri_pi) ? "compat" : "native-PCI");
	}

	/* Now the secondary controller. */
	ctl = ATA_IS_COMPAT(sec_pi) ? 0x370 : pci_device_read32(device, PCI_DEVICE_BAR2);
	cmd = ATA_IS_COMPAT(sec_pi) ? 0x170 : pci_device_read32(device, PCI_DEVICE_BAR3);
	irq = ATA_IS_COMPAT(sec_pi) ? 15    : pci_device_read8 (device, PCI_DEVICE_INTERRUPT_LINE);
	controller = ata_controller_add(device, ctl, cmd, irq);
	if(controller) {
		kprintf(LOG_NORMAL, " secondary: %" PRId32 " (controller: %p, pi: %s)\n",
			controller->id, controller, ATA_IS_COMPAT(sec_pi) ? "compat" : "native-PCI");
	}

	return true;
}

/** PCI ID structures for lookup. */
static pci_device_id_t ata_pci_ids[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x01, 0x01, NULL },
};

/** Initialisation function for the ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_init(void) {
	pci_device_lookup(ata_pci_ids, ARRAYSZ(ata_pci_ids), ata_pci_lookup_func);
	return STATUS_SUCCESS;
}

/** Unloading function for the ATA driver.
 * @return		Status code describing result of the operation. */
static status_t ata_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("ata");
MODULE_DESC("Generic ATA device driver");
MODULE_FUNCS(ata_init, ata_unload);
MODULE_DEPS("disk", "pci");
