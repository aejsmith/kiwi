/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device library core API.
 *
 * The device library provides easier to use wrapper APIs around the raw kernel
 * device class interfaces. The kernel interfaces can be somewhat convoluted to
 * use directly as they are mostly funnelled through a small set of generic
 * APIs like kern_file_request(), device attributes, etc. The device library
 * wraps these into a more friendly set of C APIs for each device class, and
 * in some cases also adds some more functionality such as state tracking which
 * does not belong in the kernel.
 */

#pragma once

#include <kernel/device.h>

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

/**
 * Device object (opaque). This wraps a kernel device handle and also holds any
 * additional state required for a specific device class.
 *
 * Each device class provides its own typedef of this, and using these aliases
 * primarily serves as documentation as to what the type of a device is in code.
 * Since all the types are aliases of device_t, it will not prevent you from
 * using one device class with APIs for another at compile time, however all
 * class-specific APIs do perform type checking at runtime.
 */
typedef struct device device_t;

/** Device classes. */
typedef enum device_class {
    /**
     * Unknown device class. Only basic (non-class-specific) APIs can be used
     * with this.
     */
    DEVICE_CLASS_UNKNOWN = 0,

    /** Input device (input_device_*). */
    DEVICE_CLASS_INPUT = 1,

    /** Network device (net_device_*). */
    DEVICE_CLASS_NET = 2,
} device_class_t;

extern void device_close(device_t *device);
extern void device_destroy(device_t *device);

extern device_class_t device_class(device_t *device);
extern handle_t device_handle(device_t *device);

__SYS_EXTERN_C_END
