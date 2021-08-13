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

#define ATA_MODULE_NAME "ata"

struct ata_channel;
struct ata_sff_channel;

/**
 * ATA register/command definitions.
 */

/** ATA Command Registers. */
#define ATA_CMD_REG_DATA            0       /**< Data register (R/W). */
#define ATA_CMD_REG_ERROR           1       /**< Error register (R). */
#define ATA_CMD_REG_FEATURES        1       /**< Features register (W). */
#define ATA_CMD_REG_SECTOR_COUNT    2       /**< Sector Count (R/W, W on packet). */
#define ATA_CMD_REG_INT_REASON      2       /**< Interrupt Reason (R, packet only). */
#define ATA_CMD_REG_LBA_LOW         3       /**< LBA Low (R/W). */
#define ATA_CMD_REG_LBA_MID         4       /**< LBA Mid (R/W). */
#define ATA_CMD_REG_BYTE_COUNT_LOW  4       /**< Byte Count Low (R/W, packet only). */
#define ATA_CMD_REG_LBA_HIGH        5       /**< LBA High (R/W). */
#define ATA_CMD_REG_BYTE_COUNT_HIGH 5       /**< Byte Count High (R/W, packet only). */
#define ATA_CMD_REG_DEVICE          6       /**< Device register (R/W). */
#define ATA_CMD_REG_STATUS          7       /**< Status register (R). */
#define ATA_CMD_REG_CMD             7       /**< Command register (W). */

/** ATA Control Registers. */
#define ATA_CTRL_REG_ALT_STATUS     0       /**< Alternate status (R). */
#define ATA_CTRL_REG_DEV_CTRL       0       /**< Device control (W). */

/** ATA error register bits. */
#define ATA_ERROR_ABRT              (1<<2)
#define ATA_ERROR_IDNF              (1<<4)

/** ATA status register bits. */
#define ATA_STATUS_ERR              (1<<0)
#define ATA_STATUS_DRQ              (1<<3)
#define ATA_STATUS_DF               (1<<5)
#define ATA_STATUS_DRDY             (1<<6)
#define ATA_STATUS_BSY              (1<<7)

/** ATA device control register bits. */
#define ATA_DEV_CTRL_NIEN           (1<<1)
#define ATA_DEV_CTRL_SRST           (1<<2)
#define ATA_DEV_CTRL_HOB            (1<<7)

/**
 * ATA driver interface.
 */

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
} ata_sff_channel_ops_t;

/** Base ATA channel structure. */
typedef struct ata_channel {
    device_t *node;

    /** Fields to be filled out by channel driver. */
    ata_channel_ops_t *ops;             /**< Channel operations. */
    uint32_t caps;                      /**< Channel capabilities (ATA_CHANNEL_CAP_*). */
    uint8_t num_devices;                /**< Maximum number of devices on the channel. */
} ata_channel_t;

/** ATA channel capabilities. */
enum {
    ATA_CHANNEL_CAP_PIO = (1<<0),       /**< Channel supports PIO. */
    ATA_CHANNEL_CAP_DMA = (1<<1),       /**< Channel supports DMA. */
};

/** Base SFF-style ATA channel structure. */
typedef struct ata_sff_channel {
    ata_channel_t ata;
    ata_sff_channel_ops_t *ops;
} ata_sff_channel_t;

DEFINE_CLASS_CAST(ata_sff_channel, ata_channel, ata);

/** Destroys an ATA channel.
 * @see                 device_destroy().
 * @param channel       Channel to destroy. */
static inline status_t ata_channel_destroy(ata_channel_t *channel) {
    return device_destroy(channel->node);
}

extern void ata_channel_interrupt(ata_channel_t *channel);

extern status_t ata_channel_create(ata_channel_t *channel, const char *name, device_t *parent);
extern status_t ata_channel_publish(ata_channel_t *channel);

extern status_t ata_sff_channel_create(ata_sff_channel_t *channel, const char *name, device_t *parent);
