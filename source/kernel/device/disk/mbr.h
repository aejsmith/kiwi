/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               MBR partition support.
 */

#pragma once

#include <types.h>

/** MBR partition table signature. */
#define MBR_SIGNATURE               0xaa55

/** MBR partition types. */
#define MBR_PARTITION_TYPE_GPT      0xee

/** MBR partition description. */
typedef struct mbr_partition {
    uint8_t bootable;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;
    uint32_t num_sectors;
} __packed mbr_partition_t;

/** MBR structure. */
typedef struct mbr {
    uint8_t bootcode[446];
    mbr_partition_t partitions[4];
    uint16_t signature;
} __packed mbr_t;
