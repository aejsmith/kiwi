/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Input device class interface.
 */

#include <device/input.h>

#include <kernel/status.h>

#include "device.h"

/** Opens an input device by path.
 * @param path          Path to device to open.
 * @param access        Requested access access for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _device       Where to store pointer to device object.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INCORRECT_TYPE if device is not an input device.
 *                      Any other possible error from kern_device_open(). */
status_t input_device_open(const char *path, uint32_t access, uint32_t flags, input_device_t **_device) {
    status_t ret;

    handle_t handle;
    ret = kern_device_open(path, access, flags, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = input_device_from_handle(handle, _device);
    if (ret != STATUS_SUCCESS)
        kern_handle_close(ret);

    return ret;
}

/** Creates an input device object from an existing handle.
 * @param handle        Handle to device.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is not a device handle.
 *                      STATUS_INCORRECT_TYPE if device is not an input device. */
status_t input_device_from_handle(handle_t handle, input_device_t **_device) {
    // TODO: Query device class.
    return STATUS_NOT_IMPLEMENTED;
}
