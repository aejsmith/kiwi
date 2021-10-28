/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               ATA device library.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://download.xskernel.org/docs/controllers/ata_atapi/AT_Attachment_with_Packet_Interface_-_7_Volume_1-v1r4b.pdf
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://download.xskernel.org/docs/controllers/ata_atapi/AT_Attachment_with_Packet_Interface_-_7_Volume_2-v2r4b.pdf
 *
 * These are a mirror found at time of writing. The official source (the T13
 * committee) appears to have removed the original copies.
 */

#pragma once

#include <device/device.h>
#include <device/dma.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#define ATA_MODULE_NAME "ata"

struct ata_channel;
struct ata_sff_channel;

/**
 * ATA register/command definitions.
 */

/** ATA Command Registers. */
enum {
    ATA_CMD_REG_DATA                    = 0,    /**< Data register (R/W). */
    ATA_CMD_REG_ERROR                   = 1,    /**< Error register (R). */
    ATA_CMD_REG_FEATURES                = 1,    /**< Features register (W). */
    ATA_CMD_REG_SECTOR_COUNT            = 2,    /**< Sector Count (R/W, W on packet). */
    ATA_CMD_REG_INT_REASON              = 2,    /**< Interrupt Reason (R, packet only). */
    ATA_CMD_REG_LBA_LOW                 = 3,    /**< LBA Low (R/W). */
    ATA_CMD_REG_LBA_MID                 = 4,    /**< LBA Mid (R/W). */
    ATA_CMD_REG_BYTE_COUNT_LOW          = 4,    /**< Byte Count Low (R/W, packet only). */
    ATA_CMD_REG_LBA_HIGH                = 5,    /**< LBA High (R/W). */
    ATA_CMD_REG_BYTE_COUNT_HIGH         = 5,    /**< Byte Count High (R/W, packet only). */
    ATA_CMD_REG_DEVICE                  = 6,    /**< Device register (R/W). */
    ATA_CMD_REG_STATUS                  = 7,    /**< Status register (R). */
    ATA_CMD_REG_CMD                     = 7,    /**< Command register (W). */
};

/** ATA Control Registers. */
enum {
    ATA_CTRL_REG_ALT_STATUS             = 0,    /**< Alternate status (R). */
    ATA_CTRL_REG_DEV_CTRL               = 0,    /**< Device control (W). */
};

/** ATA error register bits. */
enum {
    ATA_ERROR_ABRT                      = (1<<2),
    ATA_ERROR_IDNF                      = (1<<4),
};

/** ATA status register bits. */
enum {
    ATA_STATUS_ERR                      = (1<<0),
    ATA_STATUS_DRQ                      = (1<<3),
    ATA_STATUS_DF                       = (1<<5),
    ATA_STATUS_DRDY                     = (1<<6),
    ATA_STATUS_BSY                      = (1<<7),
};

/** ATA device control register bits. */
enum {
    ATA_DEV_CTRL_NIEN                   = (1<<1),
    ATA_DEV_CTRL_SRST                   = (1<<2),
    ATA_DEV_CTRL_HOB                    = (1<<7),
};

/** ATA Commands. */
enum {
    ATA_CMD_READ_DMA                    = 0xc8,
    ATA_CMD_READ_DMA_EXT                = 0x25,
    ATA_CMD_READ_SECTORS                = 0x20,
    ATA_CMD_READ_SECTORS_EXT            = 0x24,
    ATA_CMD_WRITE_DMA                   = 0xca,
    ATA_CMD_WRITE_DMA_EXT               = 0x35,
    ATA_CMD_WRITE_SECTORS               = 0x30,
    ATA_CMD_WRITE_SECTORS_EXT           = 0x34,
    ATA_CMD_PACKET                      = 0xa0,
    ATA_CMD_IDENTIFY_PACKET_DEVICE      = 0xa1,
    ATA_CMD_IDENTIFY_DEVICE             = 0xec,
};

/** ATA identification data word indices. */
enum {
    ATA_ID_CONFIG                       = 0,
    ATA_ID_SERIAL                       = 10,
    ATA_ID_REVISION                     = 23,
    ATA_ID_MODEL                        = 27,
    ATA_ID_CAPABILITIES_1               = 49,
    ATA_ID_SECTOR_COUNT                 = 60,
    ATA_ID_MULTIWORD_DMA                = 63,
    ATA_ID_MAJOR_VERSION                = 80,
    ATA_ID_FEATURE_SET_1                = 82,
    ATA_ID_FEATURE_SET_2                = 83,
    ATA_ID_ULTRA_DMA                    = 88,
    ATA_ID_LBA48_SECTOR_COUNT           = 100,
    ATA_ID_SECTOR_SIZE                  = 106,
    ATA_ID_LOGICAL_SECTOR_SIZE          = 117,

    ATA_ID_COUNT                        = 256
};

/**
 * ATA driver interface.
 */

/** Structure containing DMA transfer parameters. */
typedef struct ata_dma_region {
    dma_ptr_t addr;                     /**< Physical destination address. */
    phys_size_t size;                   /**< Number of bytes to transfer. */
} ata_dma_region_t;

/** Operations for an ATA channel. */
typedef struct ata_channel_ops {
    /** Resets the channel.
     * @param channel       Channel to reset.
     * @return              Status code describing result of operation. */
    status_t (*reset)(struct ata_channel *channel);

    /** Get the content of the status register.
     * @note                This should not clear INTRQ, so should read the
     *                      alternate status register.
     * @param channel       Channel to get status from.
     * @return              Content of the status register. */
    uint8_t (*status)(struct ata_channel *channel);

    /** Get the content of the error register.
     * @param channel       Channel to get error from.
     * @return              Content of the error register. */
    uint8_t (*error)(struct ata_channel *channel);

    /** Get the selected device on a channel.
     * @param channel       Channel to get selected device from.
     * @return              Currently selected device number. */
    uint8_t (*selected)(struct ata_channel *channel);

    /** Change the selected device on a channel.
     * @param channel       Channel to select on.
     * @param num           Device number to select. */
    void (*select)(struct ata_channel *channel, uint8_t num);

    /** Check if a device is present.
     * @param channel       Channel to select on.
     * @param num           Device number to check.
     * @return              Whether the device is present. */
    bool (*present)(struct ata_channel *channel, uint8_t num);

    /** Issue a command to the selected device.
     * @param channel       Channel to perform command on.
     * @param cmd           Command to perform. */
    void (*command)(struct ata_channel *channel, uint8_t cmd);

    /** Set up registers for an LBA28 transfer.
     * @param channel       Channel to set up on.
     * @param device        Device number to operate on.
     * @param lba           LBA to transfer from/to.
     * @param count         Sector count. */
    void (*lba28_setup)(struct ata_channel *channel, uint8_t device, uint64_t lba, size_t count);

    /** Set up registers for an LBA48 transfer.
     * @param channel       Channel to set up on.
     * @param device        Device number to operate on.
     * @param lba           LBA to transfer from/to.
     * @param count         Sector count. */
    void (*lba48_setup)(struct ata_channel *channel, uint8_t device, uint64_t lba, size_t count);

    /**
     * Operations required on channels supporting PIO.
     */

    /** Perform a PIO data read.
     * @param channel       Channel to read from.
     * @param buf           Buffer to read into.
     * @param count         Number of bytes to read. */
    void (*read_pio)(struct ata_channel *channel, void *buf, size_t count);

    /** Perform a PIO data write.
     * @param channel       Channel to write to.
     * @param buf           Buffer to write from.
     * @param count         Number of bytes to write. */
    void (*write_pio)(struct ata_channel *channel, const void *buf, size_t count);

    /**
     * Operations required on channels supporting DMA.
     */

    /** Prepares a DMA transfer.
     * @param channel       Channel to perform DMA on.
     * @param regions       Array of transfer regions. This is guaranteed to
     *                      conform to the constraints specified in the channel.
     * @param count         Number of array entries.
     * @param is_write      Whether the transfer is a write.
     * @return              Status code describing result of operation. */
    status_t (*prepare_dma)(
        struct ata_channel *channel, const ata_dma_region_t *regions,
        size_t count, bool is_write);

    /**
     * Starts a DMA transfer. This should cause an interrupt to be raised once
     * the transfer is complete.
     *
     * @param channel       Channel to start on.
     */
    void (*start_dma)(struct ata_channel *channel);

    /**
     * Cleans up after a DMA transfer. This should ensure that no interrupts
     * will be raised related to the transfer that has just completed or been
     * cancelled.
     *
     * @param channel       Channel to clean up on.
     * 
     * @return              Status code describing result of the transfer.
     */
    status_t (*finish_dma)(struct ata_channel *channel);
} ata_channel_ops_t;

/** Operations for a SFF-style ATA channel. */
typedef struct ata_sff_channel_ops {
    /** Read from a control register.
     * @param channel       Channel to read from.
     * @param reg           Register to read from.
     * @return              Value read. */
    uint8_t (*read_ctrl)(struct ata_sff_channel *channel, uint8_t reg);

    /** Write to a control register.
     * @param channel       Channel to read from.
     * @param reg           Register to write to.
     * @param val           Value to write. */
    void (*write_ctrl)(struct ata_sff_channel *channel, uint8_t reg, uint8_t val);

    /** Read from a command register.
     * @param channel       Channel to read from.
     * @param reg           Register to read from.
     * @return              Value read. */
    uint8_t (*read_cmd)(struct ata_sff_channel *channel, uint8_t reg);

    /** Write to a command register.
     * @param channel       Channel to read from.
     * @param reg           Register to write to.
     * @param val           Value to write. */
    void (*write_cmd)(struct ata_sff_channel *channel, uint8_t reg, uint8_t val);

    /**
     * Operations required on channels supporting PIO.
     */

    /** Perform a PIO data read.
     * @see                 ata_channel_ops_t::read_pio(). */
    void (*read_pio)(struct ata_sff_channel *channel, void *buf, size_t count);

    /** Perform a PIO data write.
     * @see                 ata_channel_ops_t::write_pio(). */
    void (*write_pio)(struct ata_sff_channel *channel, const void *buf, size_t count);

    /**
     * Operations required on channels supporting DMA.
     */

    /** Prepares a DMA transfer.
     * @see                 ata_channel_ops_t::prepare_dma(). */
    status_t (*prepare_dma)(
        struct ata_sff_channel *channel, const ata_dma_region_t *regions,
        size_t count, bool is_write);

    /** Starts a DMA transfer.
     * @see                 ata_channel_ops_t::start_dma(). */
    void (*start_dma)(struct ata_sff_channel *channel);

    /** Cleans up after a DMA transfer.
     * @see                 ata_channel_ops_t::finish_dma(). */
    status_t (*finish_dma)(struct ata_sff_channel *channel);
} ata_sff_channel_ops_t;

/** Base ATA channel structure. */
typedef struct ata_channel {
    device_t *node;

    /** Fields to be filled out by channel driver. */
    const ata_channel_ops_t *ops;       /**< Channel operations. */
    uint32_t caps;                      /**< Channel capabilities (ATA_CHANNEL_CAP_*). */
    dma_constraints_t dma_constraints;  /**< DMA constraints (if ATA_CHANNEL_CAP_DMA set). */
    uint32_t dma_max_region_size;       /**< Maximum byte size of a single DMA region. */
    uint32_t dma_max_region_count;      /**< Maximum number of DMA regions in a single transfer. */

    /** Internal fields. */
    mutex_t command_lock;               /**< Lock to gain exclusive use of the channel. */
    uint8_t device_mask;                /**< Mask indicating devices present. */
    semaphore_t irq_sem;                /**< Semaphore for interrupts. */
} ata_channel_t;

/** ATA channel capabilities. */
enum {
    ATA_CHANNEL_CAP_PIO     = (1<<0),   /**< Channel supports PIO. */
    ATA_CHANNEL_CAP_DMA     = (1<<1),   /**< Channel supports DMA. */
    ATA_CHANNEL_CAP_SLAVE   = (1<<2),   /**< Channel supports slave devices. */
};

/** Base SFF-style ATA channel structure. */
typedef struct ata_sff_channel {
    ata_channel_t ata;
    const ata_sff_channel_ops_t *ops;
} ata_sff_channel_t;

DEFINE_CLASS_CAST(ata_sff_channel, ata_channel, ata);

/** Destroys an ATA channel.
 * @see                 device_destroy().
 * @param channel       Channel to destroy. */
static inline status_t ata_channel_destroy(ata_channel_t *channel) {
    return device_destroy(channel->node);
}

extern void ata_channel_irq(ata_channel_t *channel);

extern status_t ata_channel_create(ata_channel_t *channel, const char *name, device_t *parent);
extern status_t ata_channel_publish(ata_channel_t *channel);

extern status_t ata_sff_channel_create(ata_sff_channel_t *channel, const char *name, device_t *parent);
