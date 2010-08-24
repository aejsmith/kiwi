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
 * @brief		AHCI HBA functions.
 */

#include <arch/barrier.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <status.h>

#include "ahci.h"

/** Next HBA ID. */
static atomic_t next_hba_id = 0;

/** AHCI IRQ handler.
 * @param num		IRQ number.
 * @param _hba		Pointer to HBA structure.
 * @param frame		Interrupt frame (unused).
 * @return		Whether the IRQ was handled. */
static irq_result_t ahci_irq_handler(unative_t num, void *_channel, intr_frame_t *frame) {
	kprintf(LOG_DEBUG, "ahci: irq\n");
	return IRQ_UNHANDLED;
}

/** Reset an AHCI HBA.
 * @param hba		HBA to reset.
 * @return		Whether successful in performing the reset. */
static bool ahci_hba_reset(ahci_hba_t *hba) {
	/* Set AHCI Enable to 1 before resetting. Not sure if this is necessary,
	 * but one part of the spec says 'Software may perform an HBA reset
	 * prior to initializing the HBA by setting GHC.AE to 1 and then
	 * setting GHC.HR to 1 if desired'. */
	hba->regs->ghc |= AHCI_GHC_AE;

	/* Set the GHC.HR bit to 1 to reset the HBA. */
	hba->regs->ghc |= AHCI_GHC_HR;
	write_barrier();

	/* Quote: 'If the HBA has not cleared GHC.HR to 0 within 1 second of
	 * software setting GHC.HR to 1, the HBA is in hung or locked state.' */
	if(!wait_for_clear(&hba->regs->ghc, AHCI_GHC_HR, false, SECS2USECS(1))) {
		return false;
	}

	/* Set AHCI Enable again. */
	hba->regs->ghc |= AHCI_GHC_AE;
	write_barrier();
	return true;
}

/** Add a new AHCI HBA.
 * @param device	Device that was matched.
 * @param data		Unused.
 * @return		Whether the device has been claimed. */
bool ahci_hba_add(pci_device_t *device, void *data) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "ahci" } },
	};
	uint16_t pci_cmd_old, pci_cmd_new;
	char name[DEVICE_NAME_MAX];
	uint8_t i, num_ports;
	uint32_t reg_base;
	ahci_hba_t *hba;
	status_t ret;

	kprintf(LOG_NORMAL, "ahci: found AHCI HBA %d:%02x.%d (vendor: 0x%04x, id: 0x%04x)\n",
	        device->bus, device->device, device->function, device->vendor_id,
	        device->device_id);

	/* Configure the PCI device appropriately. */
	pci_cmd_old = pci_cmd_new = pci_config_read16(device, PCI_CONFIG_COMMAND);
	pci_cmd_new &= ~(PCI_COMMAND_INT_DISABLE | PCI_COMMAND_IO);
	pci_cmd_new |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY);
	if(pci_cmd_new != pci_cmd_old) {
		pci_config_write16(device, PCI_CONFIG_COMMAND, pci_cmd_new);
		kprintf(LOG_DEBUG, "ahci: reconfigured PCI device %d:%02x.%d (old: 0x%04x, new: 0x%04x)\n",
		        device->bus, device->device, device->function, pci_cmd_old, pci_cmd_new);
        }

	/* Create a structure to contain information about the HBA. */
	hba = kmalloc(sizeof(*hba), MM_SLEEP);
	hba->id = atomic_inc(&next_hba_id);
	hba->pci_device = device;
	hba->regs = NULL;
	hba->irq = device->interrupt_line;

	/* Register the IRQ handler. */
	ret = irq_register(hba->irq, ahci_irq_handler, NULL, hba);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ahci: failed to register IRQ handler %u\n", hba->irq);
		goto fail;
	}

	/* Obtain the HBA memory registers address and map them. */
	reg_base = pci_config_read32(device, PCI_CONFIG_BAR5) & PCI_MEM_ADDRESS_MASK;
	hba->regs = page_phys_map(reg_base, sizeof(ahci_hba_regs_t), MM_SLEEP);
	kprintf(LOG_DEBUG, "ahci: found HBA registers at 0x%" PRIpp ", mapped to %p\n",
	        reg_base, hba->regs);
	kprintf(LOG_DEBUG, "ahci: AHCI version is %u.%u\n",
	        ((hba->regs->vs >> 24) & 0xff) * 10 + ((hba->regs->vs >> 16) & 0xff),
	        ((hba->regs->vs >> 8) & 0xff) * 10 + (hba->regs->vs & 0xff));
	kprintf(LOG_DEBUG, "ahci: interrupt line is %u\n", hba->irq);

	/* Reset the HBA. */
	if(!ahci_hba_reset(hba)) {
		kprintf(LOG_WARN, "ahci: failed to reset HBA, unable to use it\n");
		goto fail;
	}

	/* Publish it in the device tree. */
	sprintf(name, "ahci%d", hba->id);
	ret = device_create(name, device->node, NULL, hba, attr, ARRAYSZ(attr), &hba->node);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ahci: could not create device tree node for HBA %d (%d)\n",
			hba->id, ret);
		goto fail;
	}

	/* Determine which ports are presented and create structures for them.
	 * A value of 0 for NP indicates 1 port. */
	memset(hba->ports, 0, sizeof(hba->ports));
	num_ports = 1 + ((hba->regs->cap & AHCI_CAP_NP_MASK) >> AHCI_CAP_NP_SHIFT);
	kprintf(LOG_DEBUG, "ahci: HBA %d has %u port(s)\n", hba->id, num_ports);
	for(i = 0; i < num_ports; i++) {
		if(hba->regs->pi & (1<<i)) {
			hba->ports[i] = ahci_port_add(hba, i);
		}
	}

	/* Enable interrupts. */
	hba->regs->ghc |= AHCI_GHC_IE;
	write_barrier();

	/* Finish port initialisation. */
	for(i = 0; i < num_ports; i++) {
		if(hba->ports[i]) {
			ahci_port_init(hba->ports[i]);
		}
	}
	return true;
fail:
	if(hba->regs) {
		irq_unregister(hba->irq, ahci_irq_handler, NULL, hba);
		page_phys_unmap((void *)hba->regs, sizeof(ahci_hba_regs_t), true);
	}
	kfree(hba);
	return false;
}
