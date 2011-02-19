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
 * @brief		ATA bus manager.
 */

#ifndef __DRIVERS_ATA_H
#define __DRIVERS_ATA_H

#ifndef KERNEL
# error "This header is for kernel/driver use only"
#endif

#include <cpu/intr.h>

#include <io/device.h>

#include <sync/semaphore.h>

struct ata_channel;

/** ATA Commands. */
#define ATA_CMD_READ_DMA		0xC8	/**< READ DMA. */
#define ATA_CMD_READ_DMA_EXT		0x25	/**< READ DMA EXT. */
#define ATA_CMD_READ_SECTORS		0x20	/**< READ SECTORS. */
#define ATA_CMD_READ_SECTORS_EXT	0x24	/**< READ SECTORS EXT. */
#define ATA_CMD_WRITE_DMA		0xCA	/**< WRITE DMA. */
#define ATA_CMD_WRITE_DMA_EXT		0x35	/**< WRITE DMA EXT. */
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
#define ATA_CTRL_REG_ALT_STATUS		0	/**< Alternate status (R). */
#define ATA_CTRL_REG_DEVCTRL		0	/**< Device control (W). */

/** ATA error register bits. */
#define ATA_ERR_ABRT			(1<<2)	/**< Command was aborted. */
#define ATA_ERR_IDNF			(1<<4)	/**< Address not found. */

/** ATA status register bits. */
#define ATA_STATUS_ERR			(1<<0)	/**< Error. */
#define ATA_STATUS_DRQ			(1<<3)	/**< Data Request. */
#define ATA_STATUS_DF			(1<<5)	/**< Device Fault. */
#define ATA_STATUS_DRDY			(1<<6)	/**< Device Ready. */
#define ATA_STATUS_BSY			(1<<7)	/**< Busy. */

/** ATA device control register bits. */
#define ATA_DEVCTRL_NIEN		(1<<1)	/**< Disable interrupts. */
#define ATA_DEVCTRL_SRST		(1<<2)	/**< Software reset. */
#define ATA_DEVCTRL_HOB			(1<<7)	/**< High order bit. */

/** Structure containing information of a DMA transfer. */
typedef struct ata_dma_transfer {
	phys_ptr_t phys;			/**< Physical destination address. */
	size_t size;				/**< Number of bytes to transfer. */
} ata_dma_transfer_t;

/** Structure containing ATA channel operations. */
typedef struct ata_channel_ops {
	/**
	 * Operations required by all channels.
	 */

	/** Reset the channel.
	 * @param channel	Channel to reset.
	 * @return		Status code describing result of operation. */
	status_t (*reset)(struct ata_channel *channel);

	/** Get the content of the status register.
	 * @note		This should not clear INTRQ, so should read the
	 *			alternate status register.
	 * @param channel	Channel to get status from.
	 * @return		Content of the status register. */
	uint8_t (*status)(struct ata_channel *channel);

	/** Get the content of the error register.
	 * @param channel	Channel to get error from.
	 * @return		Content of the error register. */
	uint8_t (*error)(struct ata_channel *channel);

	/** Get the selected device on a channel.
	 * @param channel	Channel to get selected device from.
	 * @return		Currently selected device number. */
	uint8_t (*selected)(struct ata_channel *channel);

	/** Change the selected device on a channel.
	 * @param channel	Channel to select on.
	 * @param num		Device number to select.
	 * @return		Whether the requested device is present. */
	bool (*select)(struct ata_channel *channel, uint8_t num);

	/** Execute a command.
	 * @param channel	Channel to execute on.
	 * @param cmd		Command to execute. */
	void (*command)(struct ata_channel *channel, uint8_t cmd);

	/** Set up registers for an LBA28 transfer.
	 * @param channel	Channel to set up on.
	 * @param device	Device number to operate on.
	 * @param lba		LBA to transfer from/to.
	 * @param count		Sector count. */
	void (*lba28_setup)(struct ata_channel *channel, uint8_t device, uint64_t lba, size_t count);

	/** Set up registers for an LBA48 transfer.
	 * @param channel	Channel to set up on.
	 * @param device	Device number to operate on.
	 * @param lba		LBA to transfer from/to.
	 * @param count		Sector count. */
	void (*lba48_setup)(struct ata_channel *channel, uint8_t device, uint64_t lba, size_t count);

	/**
	 * Operations required on channels supporting PIO.
	 */

	/** Perform a PIO data read.
	 * @param channel	Channel to read from.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read. */
	void (*read_pio)(struct ata_channel *channel, void *buf, size_t count);

	/** Perform a PIO data write.
	 * @param channel	Channel to write to.
	 * @param buf		Buffer to write from.
	 * @param count		Number of bytes to write. */
	void (*write_pio)(struct ata_channel *channel, const void *buf, size_t count);

	/**
	 * Operations required on channels supporting DMA.
	 */

	/** Prepare a DMA transfer.
	 * @param channel	Channel to perform on.
	 * @param vec		Array of block descriptions. Each block will
	 *			cover no more than 1 page. The contents of this
	 *			array are guaranteed to conform to the
	 *			constraints specified to ata_channel_add().
	 * @param count		Number of array entries.
	 * @param write		Whether the transfer is a write.
	 * @return		Status code describing result of operation. */
	status_t (*prepare_dma)(struct ata_channel *channel, const ata_dma_transfer_t *vec,
	                        size_t count, bool write);

	/** Start a DMA transfer.
	 * @note		This should cause an interrupt to be raised
	 *			once the transfer is complete.
	 * @param channel	Channel to start on. */
	void (*start_dma)(struct ata_channel *channel);

	/** Clean up after a DMA transfer.
	 * @param channel	Channel to clean up on.
	 * @return		Status code describing result of the transfer. */
	status_t (*finish_dma)(struct ata_channel *channel);
} ata_channel_ops_t;

/** Structure containing ATA channel operations. */
typedef struct ata_sff_channel_ops {
	/** Read from a control register.
	 * @param channel	Channel to read from.
	 * @param reg		Register to read from.
	 * @return		Value read. */
	uint8_t (*read_ctrl)(struct ata_channel *channel, int reg);

	/** Write to a control register.
	 * @param channel	Channel to read from.
	 * @param reg		Register to write to.
	 * @param val		Value to write. */
	void (*write_ctrl)(struct ata_channel *channel, int reg, uint8_t val);

	/** Read from a command register.
	 * @param channel	Channel to read from.
	 * @param reg		Register to read from.
	 * @return		Value read. */
	uint8_t (*read_cmd)(struct ata_channel *channel, int reg);

	/** Write to a command register.
	 * @param channel	Channel to read from.
	 * @param reg		Register to write to.
	 * @param val		Value to write. */
	void (*write_cmd)(struct ata_channel *channel, int reg, uint8_t val);

	/** Perform a PIO data read.
	 * @param channel	Channel to read from.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read. */
	void (*read_pio)(struct ata_channel *channel, void *buf, size_t count);

	/** Perform a PIO data write.
	 * @param channel	Channel to write to.
	 * @param buf		Buffer to write from.
	 * @param count		Number of bytes to write. */
	void (*write_pio)(struct ata_channel *channel, const void *buf, size_t count);

	/** Prepare a DMA transfer.
	 * @param channel	Channel to perform on.
	 * @param vec		Array of block descriptions. Each block will
	 *			cover no more than 1 page. The contents of this
	 *			array are guaranteed to conform to the
	 *			constraints specified to ata_channel_add().
	 * @param count		Number of array entries.
	 * @param write		Whether the transfer is a write.
	 * @return		Status code describing result of operation. */
	status_t (*prepare_dma)(struct ata_channel *channel, const ata_dma_transfer_t *vec,
	                        size_t count, bool write);

	/** Start a DMA transfer.
	 * @note		This should cause an interrupt to be raised
	 *			once the transfer is complete.
	 * @param channel	Channel to start on. */
	void (*start_dma)(struct ata_channel *channel);

	/** Clean up after a DMA transfer.
	 * @param channel	Channel to clean up on.
	 * @return		Status code describing result of the transfer. */
	status_t (*finish_dma)(struct ata_channel *channel);
} ata_sff_channel_ops_t;

/** Structure describing an ATA channel. */
typedef struct ata_channel {
	mutex_t lock;				/**< Lock to serialise channel access. */
	device_t *node;				/**< Device tree node. */
	ata_channel_ops_t *ops;			/**< Operations for the channel. */
	ata_sff_channel_ops_t *sops;		/**< SFF operations. */
	void *data;				/**< Implementation-specific data pointer. */
	uint8_t devices;			/**< Maximum number of devices supported by the channel. */
	bool pio;				/**< Whether PIO transfers are supported. */
	bool dma;				/**< Whether DMA is supported. */
	size_t max_dma_bpt;			/**< Maximum number of blocks per DMA transfer. */
	phys_ptr_t max_dma_addr;		/**< Highest physical address for DMA transfers. */
	semaphore_t irq_sem;			/**< Semaphore for IRQs. */
} ata_channel_t;

extern ata_channel_t *ata_sff_channel_add(device_t *parent, uint8_t num, ata_sff_channel_ops_t *ops,
                                          void *data, bool dma, size_t max_dma_bpt,
                                          phys_ptr_t max_dma_addr);
extern ata_channel_t *ata_channel_add(device_t *parent, const char *name, ata_channel_ops_t *ops,
                                      ata_sff_channel_ops_t *sops, void *data, uint8_t devices,
                                      bool pio, bool dma, size_t max_dma_bpt,
                                      phys_ptr_t max_dma_addr);
extern void ata_channel_scan(ata_channel_t *channel);
extern irq_result_t ata_channel_interrupt(ata_channel_t *channel);

#endif /* __DRIVERS_ATA_H */
