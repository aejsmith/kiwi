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
