/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device library internal definitions.
 */

#pragma once

#include <device/device.h>

typedef struct device_ops {
    /** Close the device (should not free). */
    void (*close)(device_t *device);
} device_ops_t;

/**
 * Internal device structure header. This is expected to be embedded at the
 * start of a class-specific structure, or used on its own if class-specific
 * data is not needed.
 */
struct device {
    handle_t handle;
    device_class_t dev_class;
    const device_ops_t *ops;
};
