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

#include <kernel/file.h>
#include <kernel/status.h>

#include <stdlib.h>
#include <string.h>

#include "device.h"

typedef struct input_device_impl {
    device_t header;

    input_device_type_t type;
} input_device_impl_t;

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
    status_t ret;

    char class_name[DEVICE_ATTR_MAX];
    ret = kern_device_attr(handle, DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, class_name, sizeof(class_name));
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (strcmp(class_name, INPUT_DEVICE_CLASS_NAME) != 0) {
        return STATUS_INCORRECT_TYPE;
    }

    input_device_impl_t *impl = malloc(sizeof(*impl));
    if (!impl)
        return STATUS_NO_MEMORY;

    impl->header.handle    = handle;
    impl->header.dev_class = DEVICE_CLASS_INPUT;
    impl->header.ops       = NULL;

    ret = kern_device_attr(handle, INPUT_DEVICE_ATTR_TYPE, DEVICE_ATTR_INT32, &impl->type, sizeof(impl->type));
    if (ret != STATUS_SUCCESS) {
        free(impl);
        return ret;
    }

    *_device = &impl->header;
    return STATUS_SUCCESS;
}

/** Get the type of an input device.
 * @param device        Device to get type of.
 * @return              Input device type. */
input_device_type_t input_device_type(input_device_t *device) {
    input_device_impl_t *impl = (input_device_impl_t *)device;
    return impl->type;
}

/**
 * Reads the next event from an input device's event queue. If no event is
 * available this will block, unless the device was opened with FILE_NONBLOCK,
 * in which case this will return STATUS_WOULD_BLOCK.
 *
 * @param device        Device to read from.
 * @param _event        Where to store details of event read.
 *
 * @return              Status code describing the result of the operation.
 */
status_t input_device_read_event(input_device_t *device, input_event_t *_event) {
    return kern_file_read(device->handle, _event, sizeof(*_event), -1, NULL);
}
