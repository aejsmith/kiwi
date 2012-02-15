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
 * @brief		AHCI port functions.
 *
 * @todo		Port multiplier support.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

#include "ahci.h"

/** Reset the channel.
 * @param channel	Channel to reset.
 * @return		Status code describing result of operation. */
static status_t ahci_ata_reset(ata_channel_t *channel) {
	ahci_port_t *port = channel->data;
	return ahci_port_reset(port);
}

/** Get the content of the status register.
 * @param channel	Channel to get status from.
 * @return		Content of the status register. */
static uint8_t ahci_ata_status(ata_channel_t *channel) {
	ahci_port_t *port = channel->data;
	return port->regs->tfd.status;
}

/** Get the content of the error register.
 * @param channel	Channel to get error from.
 * @return		Content of the error register. */
static uint8_t ahci_ata_error(ata_channel_t *channel) {
	ahci_port_t *port = channel->data;
	return port->regs->tfd.err;
}

/** Get the selected device on a channel.
 * @param channel	Channel to get selected device from.
 * @return		Currently selected device number. */
static uint8_t ahci_ata_selected(ata_channel_t *channel) {
	return 0;
}

/** Change the selected device on a channel.
 * @param channel	Channel to select on.
 * @param num		Device number to select.
 * @return		Whether the device is present. */
static bool ahci_ata_select(ata_channel_t *channel, uint8_t num) {
	assert(num == 0);
	return true;
}

/** Execute a command.
 * @param channel	Channel to execute on.
 * @param cmd		Command to execute. */
static void ahci_ata_command(ata_channel_t *channel, uint8_t cmd) {
	ahci_port_t *port = channel->data;

	/* Set up the command header. FIS length = 0x20 = 5 DWORDs.*/
	port->clist[0].cfl = 5;
	port->clist[0].a = 0;
	port->clist[0].p = 0;
	port->clist[0].r = 0;
	port->clist[0].b = 0;
	port->clist[0].c = 0;
	port->clist[0].reserved1 = 0;
	port->clist[0].pmp = 0;

	/* Set up the command table. */
	port->ctbl->cfis.type = 0x27;
	port->ctbl->cfis.pm_port = 0;
	port->ctbl->cfis.c_bit = 1;
	port->ctbl->cfis.command = cmd;

	/* Execute the command. */
	port->regs->ci = 1;
	ahci_port_flush(port);
}

/** Set up registers for an LBA28 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
static void ahci_ata_lba28_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	ahci_port_t *port = channel->data;

	memset((void *)&port->ctbl->cfis, 0, sizeof(port->ctbl->cfis));
	port->ctbl->cfis.count_0_7 = (count == 256) ? 0 : count;
	port->ctbl->cfis.lba_0_7 = lba & 0xff;
	port->ctbl->cfis.lba_8_15 = (lba >> 8) & 0xff;
	port->ctbl->cfis.lba_16_23 = (lba >> 16) & 0xff;
	port->ctbl->cfis.device = 0x40 | ((lba >> 24) & 0xf);
}

/** Set up registers for an LBA48 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
static void ahci_ata_lba48_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	ahci_port_t *port = channel->data;

	memset((void *)&port->ctbl->cfis, 0, sizeof(port->ctbl->cfis));
	if(count == 65536) {
		port->ctbl->cfis.count_0_7 = 0;
		port->ctbl->cfis.count_8_15 = 0;
	} else {
		port->ctbl->cfis.count_0_7 = count & 0xff;
		port->ctbl->cfis.count_8_15 = (count >> 8) & 0xff;
	}
	port->ctbl->cfis.lba_0_7 = lba & 0xff;
	port->ctbl->cfis.lba_8_15 = (lba >> 8) & 0xff;
	port->ctbl->cfis.lba_16_23 = (lba >> 16) & 0xff;
	port->ctbl->cfis.lba_24_31 = (lba >> 24) & 0xff;
	port->ctbl->cfis.lba_32_39 = (lba >> 32) & 0xff;
	port->ctbl->cfis.lba_40_47 = (lba >> 40) & 0xff;
	port->ctbl->cfis.device = 0x40;
}

/** Prepare a DMA transfer.
 * @param channel	Channel to perform on.
 * @param vec		Array of block descriptions. Each block will cover no
 *			more than 1 page. The contents of this array are
 *			guaranteed to conform to the constraints specified to
 *			ata_channel_add().
 * @param count		Number of array entries.
 * @param write		Whether the transfer is a write.
 * @return		Status code describing result of operation. */
static status_t ahci_ata_prepare_dma(ata_channel_t *channel, const ata_dma_transfer_t *vec,
                                     size_t count, bool write) {
	ahci_port_t *port = channel->data;
	size_t i;

	/* Set up the command header. */
	port->clist[0].w = write;
	port->clist[0].prdtl = count;
	port->clist[0].prdbc = 0;

	/* Fill out the PRDT. */
	for(i = 0; i < count; i++) {
		// TODO: DMA align constraint to ata_channel_add()?
		if(vec[i].phys & 1 || vec[i].size & 1) {
			kprintf(LOG_WARN, "ahci: can't handle address/size not 2-byte aligned!\n");
			return STATUS_NOT_SUPPORTED;
		}

		port->prdt[i].dba = vec[i].phys & 0xFFFFFFFF;
		port->prdt[i].dbau = (vec[i].phys >> 32) & 0xFFFFFFFF;
		port->prdt[i].reserved1 = 0;
		port->prdt[i].dw3 = 0;
		port->prdt[i].dbc = vec[i].size - 1;
	}

	return STATUS_SUCCESS;
}

/** Start a DMA transfer.
 * @param channel	Channel to start on. */
static void ahci_ata_start_dma(ata_channel_t *channel) {
	/* Nothing happens. */
}

/** Clean up after a DMA transfer.
 * @param channel	Channel to clean up on.
 * @return		Status code describing result of the transfer. */
static status_t ahci_ata_finish_dma(ata_channel_t *channel) {
	ahci_port_t *port = channel->data;

	if(port->error) {
		if(port->reset) {
			ahci_port_reset(port);
			port->reset = false;
		}
		port->error = false;
		return STATUS_DEVICE_ERROR;
	} else {
		return STATUS_SUCCESS;
	}
}

/** AHCI ATA channel operations. */
static ata_channel_ops_t ahci_ata_channel_ops = {
	.reset = ahci_ata_reset,
	.status = ahci_ata_status,
	.error = ahci_ata_error,
	.selected = ahci_ata_selected,
	.select = ahci_ata_select,
	.command = ahci_ata_command,
	.lba28_setup = ahci_ata_lba28_setup,
	.lba48_setup = ahci_ata_lba48_setup,
	.prepare_dma = ahci_ata_prepare_dma,
	.start_dma = ahci_ata_start_dma,
	.finish_dma = ahci_ata_finish_dma,
};

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

	port = kmalloc(sizeof(*port), MM_WAIT);
	port->num = num;
	port->parent = hba;
	port->regs = &hba->regs->ports[num];
	port->error = false;
	port->reset = false;
	port->channel = NULL;

	/* Ensure that the port is idle. */
	port->regs->cmd &= ~AHCI_PXCMD_ST;
	if(!wait_for_clear(&port->regs->cmd, AHCI_PXCMD_CR | AHCI_PXCMD_FRE | AHCI_PXCMD_FR,
	                   false, MSECS2USECS(600))) {
		kprintf(LOG_WARN, "ahci: port %u on HBA %d did not become idle\n", num, hba->id);
		return NULL;
	}

	/* Allocate a chunk of memory to use for the port structures. */
	phys_alloc(AHCI_PORT_MEM_SIZE, 0, 0, 0, (phys_ptr_t)0x100000000, MM_WAIT | MM_ZERO, &port->mem_phys);
	port->mem_virt = phys_map(port->mem_phys, AHCI_PORT_MEM_SIZE, MM_WAIT);
	virt = (ptr_t)port->mem_virt;
	port->clist = (volatile ahci_command_header_t *)virt;
	virt += sizeof(ahci_command_header_t) * AHCI_COMMAND_HEADER_COUNT;
	port->fis = (volatile ahci_fis_t *)virt;
	virt += sizeof(ahci_fis_t);
	port->ctbl = (volatile ahci_command_table_t *)virt;
	virt += sizeof(ahci_command_table_t);
	port->prdt = (volatile ahci_prd_t *)virt;

	/* Tell the HBA the physical addresses of the command list and received
	 * FIS structure. */
	phys = port->mem_phys;
	port->regs->clb = (uint32_t)phys;
	port->regs->clbu = 0;
	phys += sizeof(ahci_command_header_t) * AHCI_COMMAND_HEADER_COUNT;
	port->regs->fb = (uint32_t)phys;
	port->regs->fbu = 0;
	phys += sizeof(ahci_fis_t);
	port->clist[0].ctba = (uint32_t)phys;
	port->clist[0].ctbau = 0;

	/* Disable power management transitions for now (IPM = 3 = transitions
	 * to partial/slumber disabled). */
	port->regs->sctl |= 0x300;

	/* Clear interrupt status. */
	port->regs->is = port->regs->is;

	/* Clear error bits. */
	port->regs->serr = port->regs->serr;

	/* Power on and spin up the device if necessary. */
	if(port->regs->cmd & AHCI_PXCMD_CPD) {
		port->regs->cmd |= AHCI_PXCMD_POD;
	}
	if(hba->regs->cap & AHCI_CAP_SSS) {
		port->regs->cmd |= AHCI_PXCMD_SUD;
	}

	/* Activate the port. */
	port->regs->cmd = (port->regs->cmd & ~AHCI_PXCMD_ICC_MASK) | (1<<28);

	/* Enable FIS receive. */
	port->regs->cmd |= AHCI_PXCMD_FRE;
	ahci_port_flush(port);
	return port;
}

/** Finish AHCI port initialisation.
 * @param port		Port to initialise.
 * @return		Whether the port could be enabled. */
bool ahci_port_init(ahci_port_t *port) {
	char name[DEVICE_NAME_MAX];

	/* Start DMA engine. */
	port->regs->cmd |= AHCI_PXCMD_ST;

	/* Set which interrupts we want to know about. */
	port->regs->ie = AHCI_PORT_INTR_ERROR | AHCI_PXIE_DHRE | AHCI_PXIE_PSE |
	                 AHCI_PXIE_DSE | AHCI_PXIE_SDBE | AHCI_PXIE_DPE;
	ahci_port_flush(port);

	/* Reset the port. */
	if(ahci_port_reset(port) != STATUS_SUCCESS) {
		return false;
	}

	/* Check if a device is present. */
	port->present = (port->regs->ssts & 0xF) == 0x3 &&
	                !(port->regs->tfd.status & ATA_STATUS_BSY) &&
	                !(port->regs->tfd.status & ATA_STATUS_DRQ);
	if(port->present) {
		// TODO: ATAPI.
		if(port->regs->sig == 0xEB140101) {
			kprintf(LOG_WARN, "ahci: ignoring unsupported ATAPI device on port %u (TODO)\n", port->num);
			return false;
		}

		/* Register the ATA channel. */
		sprintf(name, "%u", port->num);
		port->channel = ata_channel_add(port->parent->node, name, &ahci_ata_channel_ops,
		                                NULL, port, 1, false, true, AHCI_PRD_COUNT, 0);
		if(!port->channel) {
			return false;
		}

		port->error = false;
		port->reset = false;
		ata_channel_scan(port->channel);
		return true;
	} else {
		return false;
	}
}

/** Stop an AHCI port and free data associated with it.
 * @param port		Port to stop. */
void ahci_port_destroy(ahci_port_t *port) {
	/* Disable DMA engine and FIS receive. */
	port->regs->cmd &= ~(AHCI_PXCMD_ST | AHCI_PXCMD_FRE);
	ahci_port_flush(port);
	wait_for_clear(&port->regs->cmd, AHCI_PXCMD_CR | AHCI_PXCMD_FR, false, MSECS2USECS(600));

	/* Disable interrupts and clear any pending. */
	port->regs->ie = 0;
	port->regs->is = port->regs->is;
	ahci_port_flush(port);

	/* Clear addresses of our structures. */
	port->regs->clb = 0;
	port->regs->clbu = 0;
	port->regs->fb = 0;
	port->regs->fbu = 0;
	ahci_port_flush(port);

	/* Free the structure. */
	phys_unmap((void *)port->mem_virt, AHCI_PORT_MEM_SIZE, true);
	phys_free(port->mem_phys, AHCI_PORT_MEM_SIZE);
	kfree(port);
}

/** Reset an AHCI port.
 * @param port		Port to reset.
 * @return		Status code describing result of operation. */
status_t ahci_port_reset(ahci_port_t *port) {
	port->regs->cmd &= ~AHCI_PXCMD_ST;
	wait_for_clear(&port->regs->cmd, AHCI_PXCMD_CR, false, MSECS2USECS(600));

	/* Reset the device. */
	port->regs->sctl = (port->regs->sctl & ~0xF) | 1;
	ahci_port_flush(port);
	delay(1500);
	port->regs->sctl &= ~0xF;
	ahci_port_flush(port);

	/* Wait for the device to be detected. */
	wait_for_set(&port->regs->ssts, 0x1, false, MSECS2USECS(600));

	/* Clear error. */
	port->regs->serr = port->regs->serr;
	ahci_port_flush(port);

	/* Wait for communication to be established with device. */
	if(port->regs->ssts & 1) {
		if(!wait_for_set(&port->regs->ssts, 0x3, false, MSECS2USECS(600))) {
			kprintf(LOG_WARN, "ahci: device present but no Phy communication\n");
			return STATUS_DEVICE_ERROR;
		}
		port->regs->serr = port->regs->serr;
		ahci_port_flush(port);
	}

	/* Wait for the device to come back up. */
	if((port->regs->tfd.status & 0xFF) == 0xFF) {
		delay(MSECS2USECS(500));
		if((port->regs->tfd.status & 0xFF) == 0xFF) {
			kprintf(LOG_WARN, "ahci: device did not come back up after reset\n");
			return STATUS_DEVICE_ERROR;
		}
	}

	if(!wait_for_clear(&port->regs->_tfd, ATA_STATUS_BSY, false, SECS2USECS(5))) {
		kprintf(LOG_WARN, "ahci: device did not become un-busy after reset\n");
		return STATUS_DEVICE_ERROR;
	}

	/* Re-enable the DMA engine. */
	port->regs->cmd |= AHCI_PXCMD_ST;
	ahci_port_flush(port);
	return STATUS_SUCCESS;
}

/** Handle an IRQ on an AHCI port.
 * @param port		Port to handle on. */
void ahci_port_interrupt(ahci_port_t *port) {
	uint8_t is = port->regs->is;
	bool signal = false;

	if(!is) {
		return;
	}

	/* Clear pending interrupt. */
	port->regs->is = is;

	/* Handle error interrupts. */
	if(is & AHCI_PORT_INTR_ERROR) {
		port->regs->serr = port->regs->serr;
		if(is & AHCI_PXIS_UFS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Unknown FIS\n", port->parent->id, port->num);
			port->reset = true;
		}
		if(is & AHCI_PXIS_IPMS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Incorrect Port Multiplier\n", port->parent->id, port->num);
		}
		if(is & AHCI_PXIS_OFS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Overflow\n", port->parent->id, port->num);
			port->reset = true;
			signal = true;
		}
		if(is & AHCI_PXIS_INFS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Interface Non-Fatal Error\n", port->parent->id, port->num);
		}
		if(is & AHCI_PXIS_IFS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Interface Fatal Error\n", port->parent->id, port->num);
			port->reset = true;
			signal = true;
		}
		if(is & AHCI_PXIS_HBDS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Host Bus Data Error\n", port->parent->id, port->num);
			port->reset = true;
			signal = true;
		}
		if(is & AHCI_PXIS_HBFS) {
			kprintf(LOG_WARN, "ahci: %d:%u: Host Bus Fatal Error\n", port->parent->id, port->num);
			port->reset = true;
			signal = true;
		}
		if(is & AHCI_PXIS_TFES) {
			kprintf(LOG_WARN, "ahci: %d:%u: Task File Error\n", port->parent->id, port->num);
			port->reset = true;
			signal = true;
		}

		if(signal) {
			port->error = true;
		}
	} else {
		signal = true;
	}

	/* Signal the ATA stack if required. */
	if(signal && port->channel) {
		ata_channel_interrupt(port->channel);
	}
}
