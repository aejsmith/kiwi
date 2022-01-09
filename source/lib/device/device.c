/*
 * Copyright (C) 2009-2022 Alex Smith
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
 */

#include <kernel/object.h>

#include <stdlib.h>

#include "device.h"

/**
 * Closes a device object. This will close the device handle that the object is
 * wrapping, and destroy the object.
 *
 * @param device        Device object to close.
 */
void device_close(device_t *device) {
    handle_t handle = device->handle;
    device_destroy(device);
    kern_handle_close(handle);
}

/**
 * Destroys a device object without closing the underlying device handle. This
 * is useful to free the object when the underlying handle has already been
 * closed or the caller wants to continue using the handle.
 *
 * @param device        Device object to destroy.
 */
void device_destroy(device_t *device) {
    if (device->ops && device->ops->close)
        device->ops->close(device);

    free(device);
}

/** Gets the class of a device.
 * @param device        Device object.
 * @return              Class of the device. */
device_class_t device_class(device_t *device) {
    return device->dev_class;
}

/** Gets the underlying handle for a device object.
 * @param device        Device object.
 * @return              Class of the device. */
handle_t device_handle(device_t *device) {
    return device->handle;
}
