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
 * @brief		ATA bus manager private functions.
 */

#ifndef __ATA_PRIV_H
#define __ATA_PRIV_H

#include <drivers/ata.h>
#include <drivers/disk.h>

/** Structure describing an ATA device. */
typedef struct ata_device {
	uint8_t num;			/**< Device number on the controller. */
	ata_channel_t *parent;		/**< Controller containing the device. */
	device_t *node;			/**< Device tree node. */
	char model[41];			/**< Device model number. */
	char serial[21];		/**< Serial number. */
	char revision[8];		/**< Device revision. */

	/** Information about whet the device supports. */
	unsigned lba48 : 1;		/**< Whether the device supports LBA48. */
	unsigned dma : 1;		/**< Whether the device supports DMA. */
} ata_device_t;

extern uint8_t ata_channel_read_ctrl(ata_channel_t *channel, int reg);
extern void ata_channel_write_ctrl(ata_channel_t *channel, int reg, uint8_t val);
extern uint8_t ata_channel_read_cmd(ata_channel_t *channel, int reg);
extern void ata_channel_write_cmd(ata_channel_t *channel, int reg, uint8_t val);
extern status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count);
extern status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count);
extern uint8_t ata_channel_status(ata_channel_t *channel);
extern uint8_t ata_channel_selected(ata_channel_t *channel);
extern void ata_channel_command(ata_channel_t *channel, uint8_t cmd);
extern void ata_channel_reset(ata_channel_t *channel);
extern status_t ata_channel_wait(ata_channel_t *channel, uint8_t set, uint8_t clear, bool any,
                                 bool error, useconds_t timeout);
extern status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num);
extern void ata_channel_finish_command(ata_channel_t *channel);

extern void ata_device_detect(ata_channel_t *channel, uint8_t num);

#endif /* __ATA_PRIV_H */
