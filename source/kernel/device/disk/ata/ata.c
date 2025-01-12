/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ATA device library.
 */

#include <module.h>
#include <status.h>

#include "ata.h"

static status_t ata_init(void) {
    return STATUS_SUCCESS;
}

static status_t ata_unload(void) {
    return STATUS_SUCCESS;
}

MODULE_NAME(ATA_MODULE_NAME);
MODULE_DESC("ATA device library");
MODULE_FUNCS(ata_init, ata_unload);
MODULE_DEPS(DISK_MODULE_NAME);
