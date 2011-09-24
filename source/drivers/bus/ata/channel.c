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
 * @brief		ATA channel management.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/mmu.h>

#include <assert.h>
#include <kernel.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "ata_priv.h"

/** Wait for DRQ and perform a PIO data read.
 * @param channel	Channel to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count) {
	status_t ret;

	assert(channel->pio);
	assert(channel->ops->read_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->read_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Wait for DRQ and perform a PIO data write.
 * @param channel	Channel to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
	status_t ret;

	assert(channel->pio);
	assert(channel->ops->write_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->write_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Add an entry to a DMA transfer array.
 * @param vecp		Pointer to pointer to array.
 * @param entriesp	Pointer to entry count.
 * @param addr		Virtual address.
 * @param size		Size of transfer. */
static void add_dma_transfer(ata_dma_transfer_t **vecp, size_t *entriesp, ptr_t addr, size_t size) {
	size_t i = (*entriesp)++;
	phys_ptr_t phys;
	ptr_t pgoff;

	/* Find the physical address. */
	pgoff = addr % PAGE_SIZE;
	if(!mmu_context_query(&kernel_mmu_context, addr - pgoff, &phys, NULL, NULL)) {
		fatal("Part of DMA transfer buffer was not mapped");
	}

	*vecp = krealloc(*vecp, sizeof(**vecp) * *entriesp, MM_SLEEP);
	(*vecp)[i].phys = phys + pgoff;
	(*vecp)[i].size = size;
}

/** Prepare a DMA transfer.
 * @todo		This needs to properly handle the case where we break
 *			the channel's constraints on DMA data.
 * @todo		This (and the above function) is an abomination.
 * @param channel	Channel to prepare on.
 * @param buf		Buffer to transfer from/to.
 * @param count		Number of bytes to transfer.
 * @param write		Whether the transfer is a write.
 * @return		Status code describing result of operation. */
status_t ata_channel_prepare_dma(ata_channel_t *channel, void *buf, size_t count, bool write) {
	ata_dma_transfer_t *vec = NULL;
	size_t entries = 0, esize;
	ptr_t ibuf = (ptr_t)buf;
	status_t ret;

	assert(channel->dma);
	assert(channel->ops->prepare_dma);

	mmu_context_lock(&kernel_mmu_context);

	/* Align on a page boundary. */
	if(ibuf % PAGE_SIZE) {
		esize = MIN(count, ROUND_UP(ibuf, PAGE_SIZE) - ibuf);
		add_dma_transfer(&vec, &entries, ibuf, esize);
		ibuf += esize;
		count -= esize;
	}

	/* Add whole pages. */
	while(count / PAGE_SIZE) {
		add_dma_transfer(&vec, &entries, ibuf, PAGE_SIZE);
		ibuf += PAGE_SIZE;
		count -= PAGE_SIZE;
	}

	/* Add what's left. */
	if(count) {
		add_dma_transfer(&vec, &entries, ibuf, count);
	}

	mmu_context_unlock(&kernel_mmu_context);

	if(entries > channel->max_dma_bpt) {
		kprintf(LOG_WARN, "ata: ???\n");
		return STATUS_NOT_IMPLEMENTED;
	}

	/* Prepare the transfer. */
	ret = channel->ops->prepare_dma(channel, vec, entries, write);
	kfree(vec);
	return ret;
}

/** Start a DMA transfer and wait for it to complete.
 * @param channel	Channel to perform transfer on.
 * @return		True if completed, false if timed out. The operation
 *			may not have succeeded - use the result of
 *			ata_channel_finish_dma() to find out if it did. */
bool ata_channel_perform_dma(ata_channel_t *channel) {
	status_t ret;

	assert(channel->dma);
	assert(channel->ops->start_dma);

	/* Start off the transfer. */
	channel->ops->start_dma(channel);

	/* Wait for an IRQ to arrive. */
	ret = semaphore_down_etc(&channel->irq_sem, SECS2USECS(10), 0);
	return (ret == STATUS_SUCCESS);
}

/** Clean up after a DMA transfer.
 * @param channel	Channel to clean up on.
 * @return		STATUS_SUCCESS if the DMA transfer was successful,
 *			STATUS_DEVICE_ERROR if not. */
status_t ata_channel_finish_dma(ata_channel_t *channel) {
	assert(channel->dma);
	assert(channel->ops->finish_dma);
	return channel->ops->finish_dma(channel);
}

/** Get the content of the alternate status register.
 * @param channel	Channel to get from.
 * @return		Value of alternate status register. */
uint8_t ata_channel_status(ata_channel_t *channel) {
	assert(channel->ops->status);
	return channel->ops->status(channel);
}

/** Get the content of the error register.
 * @param channel	Channel to get from.
 * @return		Value of error register. */
uint8_t ata_channel_error(ata_channel_t *channel) {
	assert(channel->ops->error);
	return channel->ops->error(channel);
}

/** Get the currently selected device.
 * @param channel	Channel to get from.
 * @return		Current selected device. */
uint8_t ata_channel_selected(ata_channel_t *channel) {
	assert(channel->ops->selected);
	return channel->ops->selected(channel);
}

/** Issue a command to the selected device.
 * @param channel	Channel to perform command on.
 * @param cmd		Command to perform. */
void ata_channel_command(ata_channel_t *channel, uint8_t cmd) {
	assert(channel->ops->command);
	channel->ops->command(channel, cmd);
	spin(1);
}

/** Set up registers for an LBA28 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
void ata_channel_lba28_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	assert(channel->ops->lba28_setup);
	channel->ops->lba28_setup(channel, device, lba, count);
}

/** Set up registers for an LBA48 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
void ata_channel_lba48_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	assert(channel->ops->lba48_setup);
	channel->ops->lba48_setup(channel, device, lba, count);
}

/** Trigger a software reset of both devices.
 * @param channel	Channel to reset.
 * @return		Status code describing result of the operation. */
status_t ata_channel_reset(ata_channel_t *channel) {
	assert(channel->ops->reset);
	return channel->ops->reset(channel);
}

/** Wait for device status to change.
 * @note		When BSY is set in the status register, other bits must
 *			be ignored. Therefore, if waiting for BSY, it must be
 *			the only bit specified to wait for (unless any is true).
 *			There is also no need to wait for BSY to be cleared, as
 *			this is done automatically.
 * @param channel	Channel to wait on.
 * @param set		Bits to wait to be set.
 * @param clear		Bits to wait to be clear.
 * @param any		Wait for any bit in set to be set.
 * @param error		Check for errors/faults.
 * @param timeout	Timeout in microseconds.
 * @return		Status code describing result of the operation. */
status_t ata_channel_wait(ata_channel_t *channel, uint8_t set, uint8_t clear, bool any,
                          bool error, useconds_t timeout) {
	useconds_t i, elapsed = 0;
	uint8_t status;

	assert(timeout);

	/* If waiting for BSY, ensure no other bits are set. Otherwise, add BSY
	 * to the bits to wait to be clear. */
	if(set & ATA_STATUS_BSY) {
		assert(any || (set == ATA_STATUS_BSY && clear == 0));
	} else {
		clear |= ATA_STATUS_BSY;
	}

	while(timeout) {
		status = ata_channel_status(channel);
		if(error) {
			if(!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_ERR || status & ATA_STATUS_DF)) {
				return STATUS_DEVICE_ERROR;
			}
		}
		if(!(status & clear) && ((any && (status & set)) || (status & set) == set)) {
			return STATUS_SUCCESS;
		}

		if(elapsed < 1000) {
			i = (timeout < 10) ? timeout : 10;
			spin(i);
		} else {
			i = (timeout < 1000) ? timeout : 1000;
			usleep(i);
		}
		timeout -= i;
		elapsed += i;
	}

	return STATUS_TIMED_OUT;
}

/**
 * Prepares to perform a command on a channel.
 *
 * Locks the channel, waits for it to become ready (DRQ and BSY set to 0),
 * selects the specified device and waits for it to become ready again. This
 * implements the HI1:Check_Status and HI2:Device_Select parts of the Bus idle
 * protocol. It should be called prior to performing any command. When the
 * command is finished, ata_channel_finish_command() must be called.
 *
 * @param channel	Channel to perform command on.
 * @param num		Device number to select (0 or 1).
 *
 * @return		Status code describing the result of the operation.
 */
status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num) {
	bool attempted = false;

	assert(num < channel->devices);

	/* Begin by locking the channel, to prevent other devices on it from
	 * interfering with our operation. */
	mutex_lock(&channel->lock);

	/* Clear any pending interrupts. */
	while(semaphore_down_etc(&channel->irq_sem, 0, 0) == STATUS_SUCCESS);

	while(true) {
		/* Wait for BSY and DRQ to be cleared (BSY is checked automatically). */
		if(ata_channel_wait(channel, 0, ATA_STATUS_DRQ, false, false, SECS2USECS(5)) != STATUS_SUCCESS) {
			kprintf(LOG_WARN, "ata: timed out while waiting for channel %s to become idle (status: 0x%x)\n",
			        channel->node->name, ata_channel_status(channel));
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Check whether the device is selected. */
		if(ata_channel_selected(channel) == num) {
			return STATUS_SUCCESS;
		}

		/* Fail if we've already attempted to set the device. */
		if(attempted) {
			kprintf(LOG_WARN, "ata: channel %s did not respond to setting device %u\n",
			        channel->node->name, num);
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Try to set it and then wait again. */
		if(!channel->ops->select(channel, num)) {
			mutex_unlock(&channel->lock);
			return STATUS_NOT_FOUND;
		}

		spin(1);
	}
}

/** Releases the channel after a command.
 * @param channel	Channel to finish on. */
void ata_channel_finish_command(ata_channel_t *channel) {
	mutex_unlock(&channel->lock);
}

/** Register a new ATA channel.
 * @param parent	Parent in the device tree.
 * @param name		Name to give the device tree entry.
 * @param ops		Channel operations structure.
 * @param sops		SFF operations structure (should be NULL, use
 *			ata_sff_channel_add() instead).
 * @param data		Implementation-specific data pointer.
 * @param devices	Maximum number of devices supported by the channel.
 * @param pio		Whether the channel supports PIO. If false, DMA will be
 *			used to transfer data for commands that use the PIO
 *			protocol.
 * @param dma		Whether the channel supports DMA.
 * @param max_dma_bpt	Maximum number of blocks per DMA transfer.
 * @param max_dma_addr	Maximum physical address for a DMA transfer, or 0 if
 *			no maximum.
 * @return		Pointer to channel structure if added, NULL if not. */
ata_channel_t *ata_channel_add(device_t *parent, const char *name, ata_channel_ops_t *ops,
                               ata_sff_channel_ops_t *sops, void *data, uint8_t devices,
                               bool pio, bool dma, size_t max_dma_bpt, phys_ptr_t max_dma_addr) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "ata-channel" } },
	};
	ata_channel_t *channel;
	status_t ret;

	assert(parent);
	assert(name);
	assert(ops);
	assert(pio || dma);

	/* Create a new channel structure. */
	channel = kmalloc(sizeof(*channel), MM_SLEEP);
	mutex_init(&channel->lock, "ata_channel_lock", 0);
	semaphore_init(&channel->irq_sem, "ata_channel_irq_sem", 0);
	channel->ops = ops;
	channel->sops = sops;
	channel->data = data;
	channel->devices = devices;
	channel->pio = pio;
	channel->dma = dma;
	channel->max_dma_bpt = max_dma_bpt;
	channel->max_dma_addr = max_dma_addr;

	/* Reset the channel to a decent state. */
	ret = ata_channel_reset(channel);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: failed to reset channel %s (%d)\n", name, ret);
		kfree(channel);
		return NULL;
	}

	/* Publish it in the device tree. */
	ret = device_create(name, parent, NULL, NULL, attr, ARRAYSZ(attr), &channel->node);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: could not create device tree node for channel %s (%d)\n",
			name, ret);
		kfree(channel);
		return NULL;
	}

	return channel;
}
MODULE_EXPORT(ata_channel_add);

/** Scan an ATA channel for devices.
 * @param channel	Channel to scan. */
void ata_channel_scan(ata_channel_t *channel) {
	uint8_t i;

	for(i = 0; i < channel->devices; i++) {
		ata_device_detect(channel, i);
	}
}
MODULE_EXPORT(ata_channel_scan);

/** Handle an ATA channel interrupt.
 * @note		The caller should check that the interrupt belongs to
 *			the channel before calling this.
 * @note		Safe to call from IRQ context.
 * @param channel	Channel that the interrupt occurred on.
 * @return		IRQ result code. */
irq_status_t ata_channel_interrupt(ata_channel_t *channel) {
	if(mutex_held(&channel->lock)) {
		semaphore_up(&channel->irq_sem, 1);
		return IRQ_HANDLED;
	} else {
		return IRQ_UNHANDLED;
	}
}
MODULE_EXPORT(ata_channel_interrupt);
