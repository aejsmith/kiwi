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
 * @brief		AHCI port functions.
 */

#include <arch/barrier.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <status.h>

#include "ahci.h"

/** Add a new AHCI port and partially initialise it.
 * @note		Once this has been performed for all ports and HBA
 *			interrupts have been enabled, ahci_port_init() must
 *			be called.
 * @param hba		HBA that the port is on.
 * @param num		Port number.
 * @return		Pointer to AHCI port structure, or NULL if failed to
 *			initialise port. */
ahci_port_t *ahci_port_add(ahci_hba_t *hba, uint8_t num) {
	ahci_port_t *port;
	phys_ptr_t phys;
	ptr_t virt;

	port = kmalloc(sizeof(*port), MM_SLEEP);
	port->id = num;
	port->parent = hba;
	port->regs = &hba->regs->ports[num];

	/* Ensure that the port is idle. */
	port->regs->cmd &= ~AHCI_PXCMD_ST;
	if(!wait_for_clear(&port->regs->cmd, AHCI_PXCMD_CR | AHCI_PXCMD_FRE | AHCI_PXCMD_FR,
	                   false, SECS2USECS(600))) {
		kprintf(LOG_WARN, "ahci: port %u on HBA %d did not become idle\n", num, hba->id);
		return NULL;
	}

	/* Allocate a chunk of memory to use for the port structures. */
	port->mem_phys = page_xalloc(AHCI_PORT_MEM_SIZE / PAGE_SIZE, 0, 0, (phys_ptr_t)0x100000000,
	                             MM_SLEEP | PM_ZERO);
	port->mem_virt = page_phys_map(port->mem_phys, AHCI_PORT_MEM_SIZE, MM_SLEEP);
	virt = (ptr_t)port->mem_virt;
	port->fis = (volatile ahci_fis_t *)virt;
	virt += sizeof(ahci_fis_t);
	port->clist = (volatile ahci_command_header_t *)virt;
	virt += sizeof(ahci_command_header_t) * AHCI_COMMAND_HEADER_COUNT;
	port->ctbl = (volatile ahci_command_table_t *)virt;
	virt += sizeof(ahci_command_table_t);
	port->prdt = (volatile ahci_prd_t *)virt;

	/* Tell the HBA the physical addresses of the command list and received
	 * FIS structure. */
	phys = port->mem_phys;
	port->regs->fbu = 0;
	port->regs->fb = phys;
	phys += sizeof(ahci_fis_t);
	port->regs->clbu = 0;
	port->regs->clb = phys;
	phys += sizeof(ahci_command_header_t) * AHCI_COMMAND_HEADER_COUNT;
	port->clist[0].ctbau = 0;
	port->clist[0].ctba = phys;

	/* Enable FIS receive. */
	port->regs->cmd |= AHCI_PXCMD_FRE;

	/* Power on and spin up the device if necessary. */
	if(port->regs->cmd & AHCI_PXCMD_CPD) {
		port->regs->cmd |= AHCI_PXCMD_POD;
	}
	if(hba->regs->cap & AHCI_CAP_SSS) {
		port->regs->cmd |= AHCI_PXCMD_SUD;
	}

	/* Disable power management transitions for now (IPM = 3 = transitions
	 * to partial/slumber disabled). */
	port->regs->sctl |= 0x300;

	/* Clear error bits. */
	port->regs->serr = port->regs->serr;

	/* Clear interrupt status. */
	port->regs->is = port->regs->is;

	/* Set which interrupts we want to know about. */
	port->regs->ie = AHCI_PORT_INTR_ERROR | AHCI_PXIE_DHRE | AHCI_PXIE_PSE |
	                 AHCI_PXIE_DSE | AHCI_PXIE_SDBE | AHCI_PXIE_DPE;
	write_barrier();
	return port;
}

/** Finish AHCI port initialisation.
 * @param port		Port to initialise. */
void ahci_port_init(ahci_port_t *port) {
	/* Check if a device is present. */
	port->present = (port->regs->ssts & 0xF) == 0x3 &&
	                !(port->regs->tfd.status & ATA_STATUS_BSY) &&
	                !(port->regs->tfd.status & ATA_STATUS_DRQ);
	if(port->present) {
		kprintf(LOG_DEBUG, "ahci: device present on port %u\n", port->id);
	}
}
