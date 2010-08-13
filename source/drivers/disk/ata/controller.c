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

#include <arch/io.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "ata_priv.h"

/** List of all ATA controllers. */
static LIST_DECLARE(ata_controllers);
static MUTEX_DECLARE(ata_controllers_lock, 0);

/** Next controller ID. */
static atomic_t next_controller_id = 0;

/** Handle an IRQ on an ATA controller.
 * @param num		Interrupt number.
 * @param data		Pointer to controller structure.
 * @param frame		Interrupt stack frame.
 * @return		IRQ status code. */
#if 0
static irq_result_t ata_controller_irq(unative_t num, void *data, intr_frame_t *frame) {
	ata_controller_t *controller = data;
	irq_result_t ret = IRQ_RESCHEDULE;

	/* The condition variable code atomically unlocks this and so
	 * guarantees it will be waiting when we get the lock. */
	spinlock_lock(&controller->irq_lock, 0);
	if(!condvar_broadcast(&controller->irq_cv)) {
		ret = IRQ_UNHANDLED;
	}
	spinlock_unlock(&controller->irq_lock);

	return ret;
}
#endif

/** Get the status of the currently selected device.
 * @param controller	Controller to check.
 * @return		Value of alternate status register. */
uint8_t ata_controller_status(ata_controller_t *controller) {
	return in8(controller->ctl_base + ATA_CTL_REG_ALT_STATUS);
}

/** Get the error of the currently selected device.
 * @param controller	Controller to check.
 * @return		Value of error register. */
uint8_t ata_controller_error(ata_controller_t *controller) {
	return in8(controller->cmd_base + ATA_CMD_REG_ERR);
}

/** Wait for device status to change.
 * @param controller	Controller to wait on.
 * @param set		Bits to wait to be set.
 * @param clear		Bits to wait to be clear.
 * @param any		Wait for any bit in set to be set.
 * @param error		Check for errors/faults.
 * @param timeout	Timeout in microseconds.
 * @return		Status code describing result of the operation. */
status_t ata_controller_wait(ata_controller_t *controller, uint8_t set, uint8_t clear,
                             bool any, bool error, useconds_t timeout) {
	useconds_t elapsed = 0, i;
	uint8_t status;

	assert(timeout);

	while(elapsed < timeout) {
		status = ata_controller_status(controller);
		if(error) {
			if(!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_ERR || status & ATA_STATUS_DF)) {
				return STATUS_DEVICE_ERROR;
			}
		}
		if(!(status & clear) && ((any && (status & set)) || (status & set) == set)) {
			return STATUS_SUCCESS;
		}

		i = (timeout < 1000) ? timeout : 1000;
		usleep(i);
		elapsed += i;
	}

	return STATUS_TIMED_OUT;
}

/** Write a command.
 * @param controller	Controller to send command to. Should be locked.
 * @param cmd		Command to send. */
void ata_controller_command(ata_controller_t *controller, uint8_t cmd) {
	out8(controller->cmd_base + ATA_CMD_REG_CMD, cmd);
}

/** Change selected device on a controller.
 *
 * Waits for the controller to become ready (DRQ and BSY set to 0), selects
 * the specified device and waits for it to become ready again. This implements
 * the HI1:Check_Status and HI2:Device_Select parts of the Bus idle protocol.
 * It should be called prior to performing any command.
 *
 * @param controller	Controller to change on. Should be locked.
 * @param num		Device number to select (0 or 1). */
void ata_controller_select(ata_controller_t *controller, uint8_t num) {
	assert(num == 0 || num == 1);

	out8(controller->cmd_base + ATA_CMD_REG_DEVICE, 0xA0 | (num << 4));
	usleep(1);
}

/** Perform a PIO data read.
 * @note		Caller should check DRQ status.
 * @param controller	Controller to read from. Should be locked.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read. */
void ata_controller_pio_read(ata_controller_t *controller, void *buf, size_t count) {
	in16s(controller->cmd_base + ATA_CMD_REG_DATA, (count / 2), (uint16_t *)buf);
}

/** Perform a PIO data write.
 * @note		Caller should check DRQ status.
 * @param controller	Controller to write to. Should be locked.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write. */
void ata_controller_pio_write(ata_controller_t *controller, const void *buf, size_t count) {
	out16s(controller->cmd_base + ATA_CMD_REG_DATA, (count / 2), (uint16_t *)buf);
}

/** Check if a controller is present and add it to the list.
 * @param device	PCI device node.
 * @param ctl		Control register base.
 * @param cmd		Command register base.
 * @param irq		IRQ number.
 * @return		Pointer to structure on success, NULL on failure. */
ata_controller_t *ata_controller_add(device_t *device, uint32_t ctl, uint32_t cmd, uint32_t irq) {
	ata_controller_t *controller;
	char name[DEVICE_NAME_MAX];
	status_t ret;

	/* Check controller presence by writing a value to the low LBA port
	 * on the controller, then read back. If the value is the same, the
	 * controller is present, else it is not. */
	out8(cmd + ATA_CMD_REG_LBA_LOW, 0xAB);
	if(in8(cmd + ATA_CMD_REG_LBA_LOW) != 0xAB) {
		return NULL;
	}

	controller = kmalloc(sizeof(ata_controller_t), MM_SLEEP);
	list_init(&controller->header);
	mutex_init(&controller->lock, "ata_controller_lock", 0);
	list_init(&controller->devices);
	spinlock_init(&controller->irq_lock, "ata_controller_irq_lock");
	condvar_init(&controller->irq_cv, "ata_controller_irq_cv");
	controller->id = atomic_inc(&next_controller_id);
	controller->pci = device;
	controller->ctl_base = ctl;
	controller->cmd_base = cmd;
	controller->irq = irq;

	/* Register the controller IRQ. */
	//if((ret = irq_register(irq, ata_controller_irq, NULL, controller)) != 0) {
	//	kprintf(LOG_WARN, "ata: warning: could not register IRQ %d\n", ret);
	//	kfree(controller);
	//	return NULL;
	//}

	/* Add it under the PCI device node. */
	sprintf(name, "ata%d", controller->id);
	ret = device_create(name, controller->pci, NULL, NULL, NULL, 0, &controller->device);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: could not create device node for %" PRId32 " (%d)\n",
			controller->id, ret);
		//irq_unregister(irq, ata_controller_irq, NULL, controller);
		kfree(controller);
		return NULL;
	}

	mutex_lock(&ata_controllers_lock);
	list_append(&ata_controllers, &controller->header);
	mutex_unlock(&ata_controllers_lock);
	return controller;
}

/** Scan a controller for devices.
 * @param controller	Controller to scan. */
void ata_controller_scan(ata_controller_t *controller) {
	ata_device_detect(controller, 0);
	ata_device_detect(controller, 1);
}
