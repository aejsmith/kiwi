/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Disk device class interface.
 *
 * Supported standard operations:
 *  - kern_file_read():
 *    Reads data from the disk (if media is present)
 *  - kern_file_write():
 *    Writes data to the disk (if media is present and is writeable).
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** Disk device class name. */
#define DISK_DEVICE_CLASS_NAME      "disk"

__KERNEL_EXTERN_C_END
