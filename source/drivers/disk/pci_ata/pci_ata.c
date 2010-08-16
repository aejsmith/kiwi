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

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>

/** Structure containing PCI ATA channel information. */
typedef struct pci_ata_channel {
	pci_device_t *pci_device;	/**< PCI device of the controller. */
	ata_channel_t *channel;		/**< ATA bus manager channel structure. */
	uint32_t ctrl_base;		/**< Control register base. */
	uint32_t cmd_base;		/**< Command register base. */
	uint32_t irq;			/**< IRQ number. */
} pci_ata_channel_t;

/** Check if a channel is in compatibility mode. */
#define PCI_ATA_IS_COMPAT(pi)	((pi) == 0x00 || (pi) == 0x02)

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

/** PCI ATA channel operations. */
static ata_channel_ops_t pci_ata_channel_ops = {
	.read_ctrl = pci_ata_channel_read_ctrl,
	.write_ctrl = pci_ata_channel_write_ctrl,
	.read_cmd = pci_ata_channel_read_cmd,
	.write_cmd = pci_ata_channel_write_cmd,
	.read_pio = pci_ata_channel_read_pio,
	.write_pio = pci_ata_channel_write_pio,
};

/** Handler for a PCI ATA IRQ.
 * @param num		IRQ number.
 * @param _channel	Pointer to channel structure.
 * @param frame		Interrupt frame (unused).
 * @return		Whether the IRQ was handled. */
static irq_result_t pci_ata_irq_handler(unative_t num, void *_channel, intr_frame_t *frame) {
	pci_ata_channel_t *channel = _channel;

	if(!channel->channel) {
		return IRQ_UNHANDLED;
	}

	kprintf(LOG_DEBUG, "ata: received PCI IRQ\n");
	return IRQ_UNHANDLED;
}

/** Register a new PCI ATA channel.
 * @param pci_device	PCI device the channel is in.
 * @param ctrl_base	Control registers base address.
 * @param cmd_base	Command registers base address.
 * @param irq		IRQ number.
 * @return		Pointer to ATA channel structure if present. */
static ata_channel_t *pci_ata_channel_add(pci_device_t *pci_device, uint32_t ctrl_base,
                                          uint32_t cmd_base, uint32_t irq) {
	uint16_t pci_cmd_old, pci_cmd_new;
	pci_ata_channel_t *channel;
	status_t ret;

	/* Configure the PCI device appropriately. */
	pci_cmd_old = pci_cmd_new = pci_config_read16(pci_device, PCI_CONFIG_COMMAND);
	if(pci_cmd_new & PCI_COMMAND_INT_DISABLE) {
		pci_cmd_new &= ~PCI_COMMAND_INT_DISABLE;
	}
	if(!(pci_cmd_new & PCI_COMMAND_IO)) {
		pci_cmd_new |= PCI_COMMAND_IO;
	}
	if(!(pci_cmd_new & PCI_COMMAND_BUS_MASTER)) {
		pci_cmd_new |= PCI_COMMAND_BUS_MASTER;
	}
	if(pci_cmd_new != pci_cmd_old) {
		pci_config_write16(pci_device, PCI_CONFIG_COMMAND, pci_cmd_new);
		kprintf(LOG_DEBUG, "ata: reconfigured PCI device %d:%02x.%d (old: 0x%04x, new: 0x%04x)\n",
		        pci_device->bus, pci_device->device, pci_device->function,
		        pci_cmd_old, pci_cmd_new);
        }

	/* Allocate our information structure. */
	channel = kmalloc(sizeof(*channel), MM_SLEEP);
	channel->channel = NULL;
	channel->pci_device = pci_device;
	channel->ctrl_base = ctrl_base;
	channel->cmd_base = cmd_base;
	channel->irq = irq;

	/* Register the IRQ handler. */
	ret = irq_register(channel->irq, pci_ata_irq_handler, NULL, channel);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: failed to register PCI ATA IRQ handler %u\n", channel->irq);
		kfree(channel);
		return NULL;
	}

	/* Try to register the ATA channel. */
	channel->channel = ata_channel_add(pci_device->node, &pci_ata_channel_ops, channel);
	if(!channel->channel) {
		irq_unregister(channel->irq, pci_ata_irq_handler, NULL, channel);
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
	uint32_t ctrl_base, cmd_base, irq;
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

	/* Get primary channel details and add it. */
	if(PCI_ATA_IS_COMPAT(pri_pi)) {
		/* Compatibility-mode channels always have the same details. */
		ctrl_base = 0x3F6;
		cmd_base = 0x1F0;
		irq = 14;
	} else {
		/* Quote: "Base registers used to map Control Block registers
		 * must ask for 4 bytes of IO space. In this four byte
		 * allocation the byte at offset 02h is where the Alternate
		 * Status/Device Control byte is located.". Therefore, add 2
		 * to the value read. */
		ctrl_base = pci_config_read32(device, PCI_CONFIG_BAR0) + 2;
		cmd_base = pci_config_read32(device, PCI_CONFIG_BAR1);
		irq = device->interrupt_line;
	}

	/* Add the channel. */
	pri = pci_ata_channel_add(device, ctrl_base, cmd_base, irq);
	if(pri) {
		kprintf(LOG_NORMAL, " primary:   %d (%s, ctrl_base: 0x%x, cmd_base: 0x%x, irq: %d)\n",
		        pri->id, PCI_ATA_IS_COMPAT(pri_pi) ? "compat" : "native-PCI",
		        ctrl_base, cmd_base, irq);
	}

	/* Now the secondary channel. */
	if(PCI_ATA_IS_COMPAT(sec_pi)) {
		ctrl_base = 0x376;
		cmd_base = 0x170;
		irq = 15;
	} else {
		/* Same as above. */
		ctrl_base = pci_config_read32(device, PCI_CONFIG_BAR2) + 2;
		cmd_base = pci_config_read32(device, PCI_CONFIG_BAR3);
		irq = device->interrupt_line;
	}

	/* Add channel if present. */
	sec = pci_ata_channel_add(device, ctrl_base, cmd_base, irq);
	if(sec) {
		kprintf(LOG_NORMAL, " secondary: %d (%s, ctrl_base: 0x%x, cmd_base: 0x%x, irq: %d)\n",
		        sec->id, PCI_ATA_IS_COMPAT(pri_pi) ? "compat" : "native-PCI",
		        ctrl_base, cmd_base, irq);
	}

	/* Scan for devices. */
	if(pri) { ata_channel_scan(pri); }
	if(sec) { ata_channel_scan(sec); }
	return true;
}

/** ATA PCI device ID structure. */
static pci_device_id_t pci_ata_device_ids[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, 0x01, 0x01, NULL },
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
MODULE_DESC("PCI ATA device driver");
MODULE_FUNCS(pci_ata_init, pci_ata_unload);
MODULE_DEPS("ata", "pci");
