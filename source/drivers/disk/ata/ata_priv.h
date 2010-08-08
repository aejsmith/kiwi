/*
 * Copyright (C) 2009 Alex Smith
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
 */

#ifndef __ATA_PRIV_H
#define __ATA_PRIV_H

#include <drivers/disk.h>
#include <drivers/pci.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

/** ATA Commands. */
#define ATA_CMD_READ_SECTORS		0x20	/**< READ SECTORS. */
#define ATA_CMD_READ_SECTORS_EXT	0x24	/**< READ SECTORS EXT. */
#define ATA_CMD_WRITE_SECTORS		0x30	/**< WRITE SECTORS. */
#define ATA_CMD_WRITE_SECTORS_EXT	0x34	/**< WRITE SECTORS EXT. */
#define ATA_CMD_PACKET			0xA0	/**< PACKET. */
#define ATA_CMD_IDENTIFY_PACKET		0xA1	/**< IDENTIFY PACKET DEVICE. */
#define ATA_CMD_IDENTIFY		0xEC	/**< IDENTIFY DEVICE. */

/** ATA Command Registers. */
#define ATA_CMD_REG_DATA		0	/**< Data register (R/W). */
#define ATA_CMD_REG_ERR			1	/**< Error register (R). */
#define ATA_CMD_REG_FEAT		1	/**< Features register (W). */
#define ATA_CMD_REG_COUNT		2	/**< Sector Count (R/W, W on packet). */
#define ATA_CMD_REG_INTR		2	/**< Interrupt Reason (R, packet only). */
#define ATA_CMD_REG_LBA_LOW		3	/**< LBA Low (R/W). */
#define ATA_CMD_REG_LBA_MID		4	/**< LBA Mid (R/W). */
#define ATA_CMD_REG_BYTE_LOW		4	/**< Byte Count Low (R/W, packet only). */
#define ATA_CMD_REG_LBA_HIGH		5	/**< LBA High (R/W). */
#define ATA_CMD_REG_BYTE_HIGH		5	/**< Byte Count High (R/W, packet only). */
#define ATA_CMD_REG_DEVICE		6	/**< Device register (R/W). */
#define ATA_CMD_REG_STATUS		7	/**< Status register (R). */
#define ATA_CMD_REG_CMD			7	/**< Command register (W). */

/** ATA Control Registers. */
#define ATA_CTL_REG_ALT_STATUS		6	/**< Alternate status (R). */
#define ATA_CTL_REG_DEVCTRL		6	/**< Device control (W). */

/** ATA error register bits. */
#define ATA_ERR_ABRT			(1<<2)	/**< Command was aborted. */
#define ATA_ERR_IDNF			(1<<4)	/**< Address not found. */

/** ATA status register bits. */
#define ATA_STATUS_ERR			(1<<0)	/**< Error. */
#define ATA_STATUS_DRQ			(1<<3)	/**< Data Request. */
#define ATA_STATUS_DF			(1<<5)	/**< Device Fault. */
#define ATA_STATUS_DRDY			(1<<6)	/**< Device Ready. */
#define ATA_STATUS_BSY			(1<<7)	/**< Busy. */

/** Structure describing an ATA controller. */
typedef struct ata_controller {
	list_t header;				/**< Controller list header. */

	int id;					/**< Number of the controller. */
	mutex_t lock;				/**< Lock to serialize access to controller. */
	device_t *pci;				/**< PCI device. */
	device_t *device;			/**< Device tree node. */
	uint32_t ctl_base;			/**< Control registers base. */
	uint32_t cmd_base;			/**< Command registers base. */
	uint32_t irq;				/**< IRQ of the controller. */
	uint8_t pi;				/**< Programming interface. */
	list_t devices;				/**< List of all devices on the controller. */
	spinlock_t irq_lock;			/**< Lock for IRQs (spinlock so can use in interrupt context). */
	condvar_t irq_cv;			/**< Condition variable to wait for IRQ on. */
} ata_controller_t;

/** Structure describing an ATA device. */
typedef struct ata_device {
	list_t header;				/**< Device list header. */

	uint8_t num;				/**< Device number on the controller. */
	ata_controller_t *parent;		/**< Controller containing the device. */
	device_t *device;			/**< Device tree node. */
	int flags;				/**< Flags for the device. */
	char model[41];				/**< Device model number. */
	char serial[21];			/**< Serial number. */
	char revision[8];			/**< Device revision. */
	uint32_t blocks;			/**< Total number of blocks. */
} ata_device_t;

/** Flags for ATA device structures. */
#define ATA_DEVICE_LBA48		(1<<1)	/**< Device supports LBA48. */

extern uint8_t ata_controller_status(ata_controller_t *controller);
extern uint8_t ata_controller_error(ata_controller_t *controller);
extern status_t ata_controller_wait(ata_controller_t *controller, uint8_t set, uint8_t clear,
                                    bool any, bool error, useconds_t timeout);
extern void ata_controller_command(ata_controller_t *controller, uint8_t cmd);
extern void ata_controller_select(ata_controller_t *controller, uint8_t num);
extern void ata_controller_pio_read(ata_controller_t *controller, void *buf, size_t count);
extern void ata_controller_pio_write(ata_controller_t *controller, const void *buf, size_t count);
extern ata_controller_t *ata_controller_add(device_t *parent, uint32_t ctl, uint32_t cmd, uint32_t irq);

extern bool ata_device_detect(ata_controller_t *controller, uint8_t num);

#endif /* __ATA_PRIV_H */
