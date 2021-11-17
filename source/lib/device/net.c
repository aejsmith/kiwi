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
 * @brief               Network device class interface.
 */

#include <device/net.h>

#include <kernel/file.h>
#include <kernel/status.h>

#include <stdlib.h>
#include <string.h>

#include "device.h"

typedef struct net_device_impl {
    device_t header;
} net_device_impl_t;

/** Opens an network device by path.
 * @param path          Path to device to open.
 * @param access        Requested access access for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _device       Where to store pointer to device object.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INCORRECT_TYPE if device is not a network device.
 *                      Any other possible error from kern_device_open(). */
status_t net_device_open(const char *path, uint32_t access, uint32_t flags, net_device_t **_device) {
    status_t ret;

    handle_t handle;
    ret = kern_device_open(path, access, flags, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = net_device_from_handle(handle, _device);
    if (ret != STATUS_SUCCESS)
        kern_handle_close(ret);

    return ret;
}

/** Creates an network device object from an existing handle.
 * @param handle        Handle to device.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is not a device handle.
 *                      STATUS_INCORRECT_TYPE if device is not a network device. */
status_t net_device_from_handle(handle_t handle, net_device_t **_device) {
    status_t ret;

    char class_name[DEVICE_ATTR_MAX];
    ret = kern_device_attr(handle, DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, class_name, sizeof(class_name));
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (strcmp(class_name, NET_DEVICE_CLASS_NAME) != 0) {
        return STATUS_INCORRECT_TYPE;
    }

    net_device_impl_t *impl = malloc(sizeof(*impl));
    if (!impl)
        return STATUS_NO_MEMORY;

    impl->header.handle    = handle;
    impl->header.dev_class = DEVICE_CLASS_NET;
    impl->header.ops       = NULL;

    *_device = &impl->header;
    return STATUS_SUCCESS;
}

/** Brings up the network interface.
 * @see                 NET_DEVICE_REQUEST_UP
 * @param device        Device to bring up.
 * @return              Status code describing the result of the operation. */
status_t net_device_up(net_device_t *device) {
    if (device->dev_class != DEVICE_CLASS_NET)
        return STATUS_INCORRECT_TYPE;

    return kern_file_request(device->handle, NET_DEVICE_REQUEST_UP, NULL, 0, NULL, 0, NULL);
}

/** Shuts down the network interface.
 * @see                 NET_DEVICE_REQUEST_DOWN
 * @param device        Device to shut down.
 * @return              Status code describing the result of the operation. */
status_t net_device_down(net_device_t *device) {
    if (device->dev_class != DEVICE_CLASS_NET)
        return STATUS_INCORRECT_TYPE;

    return kern_file_request(device->handle, NET_DEVICE_REQUEST_DOWN, NULL, 0, NULL, 0, NULL);
}

/** Adds an address to the network interface.
 * @see                 NET_DEVICE_REQUEST_ADD_ADDR
 * @param device        Device to add address to.
 * @param addr          Pointer to a net_addr_*_t structure corresponding to the
 *                      address family to add an address for. The content of
 *                      this is determined from the 'family' member at the start
 *                      of the structure.
 * @param size          Size of the structure pointed to by addr.
 * @return              Status code describing the result of the operation. */
status_t net_device_add_addr(net_device_t *device, const void *addr, size_t size) {
    if (device->dev_class != DEVICE_CLASS_NET)
        return STATUS_INCORRECT_TYPE;

    return kern_file_request(device->handle, NET_DEVICE_REQUEST_ADD_ADDR, addr, size, NULL, 0, NULL);
}

/** Removes an address from the network interface.
 * @see                 NET_DEVICE_REQUEST_REMOVE_ADDR
 * @param device        Device to remove address from.
 * @param addr          Pointer to a net_addr_*_t structure corresponding to the
 *                      address family to remove an address for. The content of
 *                      this is determined from the 'family' member at the start
 *                      of the structure.
 * @param size          Size of the structure pointed to by addr.
 * @return              Status code describing the result of the operation. */
status_t net_device_remove_addr(net_device_t *device, const void *addr, size_t size) {
    if (device->dev_class != DEVICE_CLASS_NET)
        return STATUS_INCORRECT_TYPE;

    return kern_file_request(device->handle, NET_DEVICE_REQUEST_REMOVE_ADDR, addr, size, NULL, 0, NULL);
}
