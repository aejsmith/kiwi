/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
