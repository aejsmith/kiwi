/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               VirtIO bus interface.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** VirtIO device class name. */
#define VIRTIO_DEVICE_CLASS_NAME        "virtio_device"

/** VirtIO device class attribute names. */
#define VIRTIO_DEVICE_ATTR_DEVICE_ID    "virtio_device.device_id"   /**< uint16 */

__KERNEL_EXTERN_C_END
