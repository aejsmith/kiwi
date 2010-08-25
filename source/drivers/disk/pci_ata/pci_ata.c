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
 * @brief		PCI ATA controller driver.
 *
 * Reference:
 * - PCI IDE Controller Specification
 *   http://suif.stanford.edu/~csapuntz/specs/pciide.ps
 */

#include <arch/io.h>

#include <cpu/intr.h>

#include <drivers/ata.h>
#include <drivers/pci.h>

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>

/** Structure containing a PRDT entry. */
typedef struct prdt_entry {
	uint32_t paddr;				/**< Physical address. */
	uint16_t bytes;				/**< Bytes to transfer. */
	uint16_t flags;				/**< Reserved/EOT bits. */
} __packed prdt_entry_t;

/** Structure containing PCI ATA channel information. */
typedef struct pci_ata_channel {
	pci_device_t *pci_device;		/**< PCI device of the controller. */
	ata_channel_t *channel;			/**< ATA bus manager channel structure. */
	uint32_t ctrl_base;			/**< Control register base. */
	uint32_t cmd_base;			/**< Command register base. */
	uint32_t bus_master_base;		/**< Bus master register base. */
	uint32_t irq;				/**< IRQ number. */
	prdt_entry_t *prdt;			/**< PRDT mapping. */
	phys_ptr_t prdt_phys;			/**< PRDT physical address. */
} pci_ata_channel_t;

/** Size that we allocate for the PRDT. */
#define PRDT_SIZE			PAGE_SIZE
#define PRDT_ENTRIES			(PAGE_SIZE / sizeof(prdt_entry_t))

/** PRDT flags. */
#define PRDT_EOT			(1<<15)

/** Check if a channel is in compatibility mode. */
#define PCI_ATA_IS_COMPAT(pi)		((pi) == 0x00 || (pi) == 0x02)

/** Bus master register definitions. */
#define PCI_ATA_BM_REG_CMD		0x00	/**< Command register. */
#define PCI_ATA_BM_REG_STATUS		0x02	/**< Status register. */
#define PCI_ATA_BM_REG_PRDT		0x04	/**< PRDT address. */

/** Bus master command register bit definitions. */
#define PCI_ATA_BM_CMD_RWC		(1<<3)	/**< Direction (1 = read, 0 = write). */
#define PCI_ATA_BM_CMD_START		(1<<0)	/**< Start/Stop Bus Master. */

/** Bus master status register bit definitions. */
#define PCI_ATA_BM_STATUS_ACTIVE	(1<<0)	/**< Bus Master IDE Active. */
#define PCI_ATA_BM_STATUS_ERROR		(1<<1)	/**< Error. */
#define PCI_ATA_BM_STATUS_INTERRUPT	(1<<2)	/**< Interrupt. */
#define PCI_ATA_BM_STATUS_CAPABLE0	(1<<5)	/**< Drive 0 DMA Capable. */
#define PCI_ATA_BM_STATUS_CAPABLE1	(1<<6)	/**< Drive 1 DMA Capable. */
#define PCI_ATA_BM_STATUS_SIMPLEX	(1<<7)	/**< Simplex only. */

/** Read from a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
static uint8_t pci_ata_channel_read_ctrl(ata_channel_t *channel, int reg) {
	pci_ata_channel_t *data = channel->data;
	return in8(data->ctrl_base + reg);
}

/** Write to a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
static void pci_ata_channel_write_ctrl(ata_channel_t *channel, int reg, uint8_t val) {
	pci_ata_channel_t *data = channel->data;
	out8(data->ctrl_base + reg, val);
}

/** Read from a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
static uint8_t pci_ata_channel_read_cmd(ata_channel_t *channel, int reg) {
	pci_ata_channel_t *data = channel->data;
	return in8(data->cmd_base + reg);
}

/** Write to a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
static void pci_ata_channel_write_cmd(ata_channel_t *channel, int reg, uint8_t val) {
	pci_ata_channel_t *data = channel->data;
	out8(data->cmd_base + reg, val);
}

/** Perform a PIO data read.
 * @param channel	Channel to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read. */
static void pci_ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count) {
	pci_ata_channel_t *data = channel->data;
	assert(!(count % 2));
	in16s(data->cmd_base + ATA_CMD_REG_DATA, (count / 2), (uint16_t *)buf);
}

/** Perform a PIO data write.
 * @param channel	Channel to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write. */
static void pci_ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
	pci_ata_channel_t *data = channel->data;
	assert(!(count % 2));
	out16s(data->cmd_base + ATA_CMD_REG_DATA, (count / 2), (const uint16_t *)buf);
}

/** Prepare a DMA transfer.
 * @param channel	Channel to perform on.
 * @param vec		Array of block descriptions. Each block will cover no
 *			more than 1 page.
 * @param count		Number of array entries.
	 * @param write		Whether the transfer is a write.
 * @return		Status code describing result of operation. */
static status_t pci_ata_channel_prepare_dma(ata_channel_t *channel, const ata_dma_transfer_t *vec,
                                            size_t count, bool write) {
	pci_ata_channel_t *data = channel->data;
	uint8_t status, command;
	uint32_t addr;
	size_t i;

	/* Write each vector entry into the PRDT. */
	for(i = 0; i < count; i++) {
		data->prdt[i].paddr = vec[i].phys;
		data->prdt[i].bytes = vec[i].size;
		data->prdt[i].flags = ((i + 1) == count) ? PRDT_EOT : 0;
	}

	/* Write the new PRDT address. */
	addr = in32(data->bus_master_base + PCI_ATA_BM_REG_PRDT);
	addr &= 0x3;
	addr |= data->prdt_phys;
	out32(data->bus_master_base + PCI_ATA_BM_REG_PRDT, addr);

	/* Clear error and interrupt bits. To clear the status/error bits, you
	 * have to write a 1 to them (WTF?). */
	status = in8(data->bus_master_base + PCI_ATA_BM_REG_STATUS);
	status |= (PCI_ATA_BM_STATUS_ERROR | PCI_ATA_BM_STATUS_INTERRUPT);
	out8(data->bus_master_base + PCI_ATA_BM_REG_STATUS, status);

	/* Set transfer direction. */
	command = in8(data->bus_master_base + PCI_ATA_BM_REG_CMD);
	if(write) {
		command &= ~PCI_ATA_BM_CMD_RWC;
	} else {
		command |= PCI_ATA_BM_CMD_RWC;
	}
	out8(data->bus_master_base + PCI_ATA_BM_REG_CMD, command);
	return STATUS_SUCCESS;
}

/** Start a DMA transfer.
 * @param channel	Channel to start on. */
static void pci_ata_channel_start_dma(ata_channel_t *channel) {
	pci_ata_channel_t *data = channel->data;
	uint8_t command;

	command = in8(data->bus_master_base + PCI_ATA_BM_REG_CMD);
	command |= PCI_ATA_BM_CMD_START;
	out8(data->bus_master_base + PCI_ATA_BM_REG_CMD, command);
}

/** Clean up after a DMA transfer.
 * @param channel	Channel to clean up on.
 * @return		Status code describing result of the transfer. */
static status_t pci_ata_channel_finish_dma(ata_channel_t *channel) {
	pci_ata_channel_t *data = channel->data;
	uint8_t status, command;

	status = in8(data->bus_master_base + PCI_ATA_BM_REG_STATUS);

	/* Stop the transfer. */
	command = in8(data->bus_master_base + PCI_ATA_BM_REG_CMD);
	command &= ~PCI_ATA_BM_CMD_START;
	out8(data->bus_master_base + PCI_ATA_BM_REG_CMD, command);

	/* Clear error bit. See above comment. */
	out8(data->bus_master_base + PCI_ATA_BM_REG_STATUS,
	     status | PCI_ATA_BM_STATUS_ERROR | PCI_ATA_BM_STATUS_INTERRUPT);

	/* Return the status. */
	if(status & PCI_ATA_BM_STATUS_ERROR) {
		return STATUS_DEVICE_ERROR;
	}
	return STATUS_SUCCESS;
}

/** PCI ATA channel operations. */
static ata_sff_channel_ops_t pci_ata_channel_ops = {
	.read_ctrl = pci_ata_channel_read_ctrl,
	.write_ctrl = pci_ata_channel_write_ctrl,
	.read_cmd = pci_ata_channel_read_cmd,
	.write_cmd = pci_ata_channel_write_cmd,
	.read_pio = pci_ata_channel_read_pio,
	.write_pio = pci_ata_channel_write_pio,
	.prepare_dma = pci_ata_channel_prepare_dma,
	.start_dma = pci_ata_channel_start_dma,
	.finish_dma = pci_ata_channel_finish_dma,
};

/** Handler for a PCI ATA IRQ.
 * @param num		IRQ number.
 * @param _channel	Pointer to channel structure.
 * @param frame		Interrupt frame (unused).
 * @return		Whether the IRQ was handled. */
static irq_result_t pci_ata_irq_handler(unative_t num, void *_channel, intr_frame_t *frame) {
	pci_ata_channel_t *data = _channel;
	uint8_t status;

	if(!data->channel) {
		return IRQ_UNHANDLED;
	}

	/* Check whether this device has raised an interrupt. */
	status = in8(data->bus_master_base + PCI_ATA_BM_REG_STATUS);
	if(!(status & PCI_ATA_BM_STATUS_INTERRUPT)) {
		return IRQ_UNHANDLED;
	}

	/* Clear interrupt flag. */
	out8(data->bus_master_base + PCI_ATA_BM_REG_STATUS, (status & 0xF8) | PCI_ATA_BM_STATUS_INTERRUPT);

	/* Clear INTRQ. */
	in8(data->cmd_base + ATA_CMD_REG_STATUS);

	/* Pass the interrupt to the ATA bus manager. */
	return ata_channel_interrupt(data->channel);
}

/** Register a new PCI ATA channel.
 * @param pci_device	PCI device the channel is on.
 * @param idx		Channel index.
 * @param ctrl_base	Control registers base address.
 * @param cmd_base	Command registers base address.
 * @param bm_base	Bus master base address.
 * @param irq		IRQ number.
 * @return		Pointer to ATA channel structure if present. */
static ata_channel_t *pci_ata_channel_add(pci_device_t *pci_device, int idx, uint32_t ctrl_base,
                                          uint32_t cmd_base, uint32_t bm_base, uint32_t irq) {
	uint16_t pci_cmd_old, pci_cmd_new;
	pci_ata_channel_t *channel;
	bool dma = true;
	status_t ret;

	/* Configure the PCI device appropriately. */
	pci_cmd_old = pci_cmd_new = pci_config_read16(pci_device, PCI_CONFIG_COMMAND);
	pci_cmd_new &= ~PCI_COMMAND_INT_DISABLE;
	pci_cmd_new |= (PCI_COMMAND_IO | PCI_COMMAND_BUS_MASTER);
	if(pci_cmd_new != pci_cmd_old) {
		pci_config_write16(pci_device, PCI_CONFIG_COMMAND, pci_cmd_new);
		kprintf(LOG_DEBUG, "ata: reconfigured PCI device %d:%02x.%d (old: 0x%04x, new: 0x%04x)\n",
		        pci_device->bus, pci_device->device, pci_device->function,
		        pci_cmd_old, pci_cmd_new);
        }

	/* Check presence by writing a value to the low LBA port on the channel,
	 * then reading it back. If the value is the same, it is present. */
	out8(cmd_base + ATA_CMD_REG_LBA_LOW, 0xAB);
	if(in8(cmd_base + ATA_CMD_REG_LBA_LOW) != 0xAB) {
		if(pci_cmd_new != pci_cmd_old) {
			pci_config_write16(pci_device, PCI_CONFIG_COMMAND, pci_cmd_old);
		}
		return NULL;
	}

	/* Allocate our information structure. */
	channel = kmalloc(sizeof(*channel), MM_SLEEP);
	channel->channel = NULL;
	channel->pci_device = pci_device;
	channel->ctrl_base = ctrl_base;
	channel->cmd_base = cmd_base;
	channel->bus_master_base = bm_base + (idx * 8);
	channel->irq = irq;
	channel->prdt = NULL;

	/* If the bus master is in simplex mode, disable DMA on the second
	 * channel. According to the Haiku code, Intel controllers use this for
	 * something other than simplex mode. */
	if(pci_device->vendor_id != 0x8086) {
		if(in8(bm_base + PCI_ATA_BM_REG_STATUS) & PCI_ATA_BM_STATUS_SIMPLEX && idx > 1) {
			dma = false;
		}
	}

	/* Allocate a PRDT if necessary. */
	if(dma) {
		channel->prdt_phys = page_xalloc(PRDT_SIZE / PAGE_SIZE, 0, 0, (phys_ptr_t)0x100000000, MM_SLEEP);
		channel->prdt = page_phys_map(channel->prdt_phys, PRDT_SIZE, MM_SLEEP);
	}

	/* Register the IRQ handler. */
	ret = irq_register(channel->irq, pci_ata_irq_handler, NULL, channel);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: failed to register PCI ATA IRQ handler %u\n", channel->irq);
		if(dma) {
			page_phys_unmap(channel->prdt, PRDT_SIZE, true);
			page_free(channel->prdt_phys, PRDT_SIZE / PAGE_SIZE);
		}
		kfree(channel);
		return NULL;
	}

	/* Try to register the ATA channel. */
	channel->channel = ata_sff_channel_add(pci_device->node, idx, &pci_ata_channel_ops, channel,
	                                       dma, PRDT_ENTRIES, (phys_ptr_t)0x100000000);
	if(!channel->channel) {
		irq_unregister(channel->irq, pci_ata_irq_handler, NULL, channel);
		if(dma) {
			page_phys_unmap(channel->prdt, PRDT_SIZE, true);
			page_free(channel->prdt_phys, PRDT_SIZE / PAGE_SIZE);
		}
		kfree(channel);
		return NULL;
	}

	return channel->channel;
}

/** Add a new PCI ATA device.
 * @param device	Device that was matched.
 * @param data		Unused.
 * @return		Whether the device has been claimed. */
static bool pci_ata_add_device(pci_device_t *device, void *data) {
	uint32_t ctrl_base, cmd_base, bus_master_base, irq;
	ata_channel_t *pri, *sec;
	uint8_t pri_pi, sec_pi;

	kprintf(LOG_NORMAL, "ata: found PCI ATA controller %d:%02x.%d (vendor: 0x%04x, id: 0x%04x)\n",
	        device->bus, device->device, device->function, device->vendor_id,
	        device->device_id);

	/* Get the programming interface so we can find out what mode the
	 * channels are operating in. The primary channel's interface is in the
	 * lower 2 bits, so we clear the higher 2 bits to find it. The secondary
	 * channel's interface, obviously, is in the higher 2 bits, so we we
	 * shift it right by 2 bits. */
	pri_pi = (device->prog_iface & 0x0F) & ~0x0C;
	sec_pi = (device->prog_iface & 0x0F) >> 2;

	/* Get the bus master base. */
	bus_master_base = pci_config_read32(device, PCI_CONFIG_BAR4) & PCI_IO_ADDRESS_MASK;

	/* Get primary channel details and add it. */
	if(PCI_ATA_IS_COMPAT(pri_pi)) {
		/* Compatibility mode channels always have the same details. */
		ctrl_base = 0x3F6;
		cmd_base = 0x1F0;
		irq = 14;
	} else {
		/* Quote: "Base registers used to map Control Block registers
		 * must ask for 4 bytes of IO space. In this four byte
		 * allocation the byte at offset 02h is where the Alternate
		 * Status/Device Control byte is located.". Therefore, add 2
		 * to the value read. */
		ctrl_base = (pci_config_read32(device, PCI_CONFIG_BAR1) & PCI_IO_ADDRESS_MASK) + 2;
		cmd_base = (pci_config_read32(device, PCI_CONFIG_BAR0) & PCI_IO_ADDRESS_MASK);
		irq = device->interrupt_line;
	}

	/* Add the channel. */
	pri = pci_ata_channel_add(device, 0, ctrl_base, cmd_base, bus_master_base, irq);
	if(pri) {
		kprintf(LOG_NORMAL, " primary:   %s (ctrl_base: 0x%x, cmd_base: 0x%x, bm_base: 0x%x, irq: %d)\n",
		        PCI_ATA_IS_COMPAT(pri_pi) ? "compat" : "native-PCI",
		        ctrl_base, cmd_base, bus_master_base, irq);
	}

	/* Now the secondary channel. */
	if(PCI_ATA_IS_COMPAT(sec_pi)) {
		ctrl_base = 0x376;
		cmd_base = 0x170;
		irq = 15;
	} else {
		/* Same as above. */
		ctrl_base = (pci_config_read32(device, PCI_CONFIG_BAR3) & PCI_IO_ADDRESS_MASK) + 2;
		cmd_base = (pci_config_read32(device, PCI_CONFIG_BAR2) & PCI_IO_ADDRESS_MASK);
		irq = device->interrupt_line;
	}

	/* Add the channel. */
	sec = pci_ata_channel_add(device, 1, ctrl_base, cmd_base, bus_master_base, irq);
	if(sec) {
		kprintf(LOG_NORMAL, " secondary: %s (ctrl_base: 0x%x, cmd_base: 0x%x, bm_base: 0x%x, irq: %d)\n",
		        PCI_ATA_IS_COMPAT(pri_pi) ? "compat" : "native-PCI",
		        ctrl_base, cmd_base, bus_master_base + 8, irq);
	}

	/* Scan for devices. */
	if(pri) { ata_channel_scan(pri); }
	if(sec) { ata_channel_scan(sec); }
	return true;
}

/** ATA PCI device ID table. */
static pci_device_id_t pci_ata_device_ids[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x01, 0x01, PCI_ANY_ID, NULL },
};

/** ATA PCI driver structure. */
static pci_driver_t pci_ata_driver = {
	.ids = pci_ata_device_ids,
	.count = ARRAYSZ(pci_ata_device_ids),
	.add_device = pci_ata_add_device,
};

/** Initialisation function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t pci_ata_init(void) {
	return pci_driver_register(&pci_ata_driver);
}

/** Unloading function for the PCI ATA driver.
 * @return		Status code describing result of the operation. */
static status_t pci_ata_unload(void) {
	pci_driver_unregister(&pci_ata_driver);
	return STATUS_SUCCESS;
}

MODULE_NAME("pci_ata");
MODULE_DESC("PCI ATA controller driver");
MODULE_FUNCS(pci_ata_init, pci_ata_unload);
MODULE_DEPS("ata", "pci");
