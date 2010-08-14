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
 * @brief		ATA bus manager.
 */

#ifndef __DRIVERS_ATA_H
#define __DRIVERS_ATA_H

#ifndef KERNEL
# error "This header is for kernel/driver use only"
#endif

#include <io/device.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

struct ata_channel;

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
#define ATA_CTL_REG_ALT_STATUS		0	/**< Alternate status (R). */
#define ATA_CTL_REG_DEVCTRL		0	/**< Device control (W). */

/** ATA error register bits. */
#define ATA_ERR_ABRT			(1<<2)	/**< Command was aborted. */
#define ATA_ERR_IDNF			(1<<4)	/**< Address not found. */

/** ATA status register bits. */
#define ATA_STATUS_ERR			(1<<0)	/**< Error. */
#define ATA_STATUS_DRQ			(1<<3)	/**< Data Request. */
#define ATA_STATUS_DF			(1<<5)	/**< Device Fault. */
#define ATA_STATUS_DRDY			(1<<6)	/**< Device Ready. */
#define ATA_STATUS_BSY			(1<<7)	/**< Busy. */

/** Structure containing ATA channel operations. */
typedef struct ata_channel_ops {
	/** Write to a control register.
	 * @param reg		Register to write to.
	 * @param val		Value to write. */
	//void (*write_ctrl
} ata_channel_ops_t;

/** Structure describing an ATA channel. */
typedef struct ata_channel {
	int id;					/**< ID of the channel. */
	mutex_t lock;				/**< Lock to serialise channel access. */
	device_t *node;				/**< Device tree node. */
	ata_channel_ops_t *ops;			/**< Operations for the channel. */
	void *data;				/**< Implementation-specific data pointer. */
	spinlock_t irq_lock;			/**< Lock for IRQs (spinlock so can use in interrupt context). */
	condvar_t irq_cv;			/**< Condition variable to wait for IRQ on. */
} ata_channel_t;

#if 0
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
#define ATA_DEVICE_DMA
#endif

extern ata_channel_t *ata_channel_add(device_t *parent, ata_channel_ops_t *ops, void *data);

#endif /* __DRIVERS_ATA_H */
