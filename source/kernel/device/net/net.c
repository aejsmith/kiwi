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
 * @brief               Network device class.
 *
 * TODO:
 *  - Security controls for network device requests.
 */

#include <device/net/net.h>

#include <net/family.h>

#include <device/class.h>

#include <lib/string.h>

#include <module.h>
#include <status.h>

static device_class_t net_device_class;

static void net_device_destroy_impl(device_t *node) {
    net_device_t *device = node->private;

    // TODO. Call device destroy function.
    (void)device;
    fatal("TODO");
}

static status_t request_interface_id(net_device_t *device, void **_out, size_t *_out_size) {
    status_t ret;

    net_interface_read_lock();

    if (device->interface.flags & NET_INTERFACE_UP) {
        uint32_t *id = kmalloc(sizeof(*id), MM_KERNEL);
        *id = device->interface.id;

        *_out      = id;
        *_out_size = sizeof(*id);

        ret = STATUS_SUCCESS;
    } else {
        ret = STATUS_NET_DOWN;
    }

    net_interface_unlock();
    return ret;
}

/** Copy and validate a net_interface_addr_t according to its family. */
static status_t copy_net_interface_addr(const void *in, size_t in_size, net_interface_addr_t *addr) {
    /*
     * The net_interface_addr_t structure is a kernel-internal union of all
     * supported address families. The request supplies a structure specific to
     * the address family. We first need to check the family to see what the
     * size should be for that family.
     */

    memset(addr, 0, sizeof(*addr));

    if (in_size < sizeof(sa_family_t))
        return STATUS_INVALID_ARG;

    sa_family_t id = *(const sa_family_t *)in;

    const net_family_t *family = net_family_get(id);
    if (!family) {
        return STATUS_ADDR_NOT_SUPPORTED;
    } else if (in_size != family->interface_addr_len) {
        return STATUS_INVALID_ARG;
    }

    memcpy(addr, in, in_size);
    return STATUS_SUCCESS;
}

static status_t request_add_addr(net_device_t *device, const void *in, size_t in_size) {
    net_interface_addr_t addr;
    status_t ret = copy_net_interface_addr(in, in_size, &addr);
    if (ret == STATUS_SUCCESS)
        ret = net_interface_add_addr(&device->interface, &addr);

    return ret;
}

static status_t request_remove_addr(net_device_t *device, const void *in, size_t in_size) {
    net_interface_addr_t addr;
    status_t ret = copy_net_interface_addr(in, in_size, &addr);
    if (ret == STATUS_SUCCESS)
        ret = net_interface_remove_addr(&device->interface, &addr);

    return ret;
}

/** Handler for network device-specific requests. */
static status_t net_device_request(
    device_t *node, file_handle_t *handle, unsigned request,
    const void *in, size_t in_size, void **_out, size_t *_out_size)
{
    net_device_t *device = node->private;
    status_t ret;

    switch (request) {
        case NET_DEVICE_REQUEST_UP:
            ret = net_interface_up(&device->interface);
            break;

        case NET_DEVICE_REQUEST_DOWN:
            ret = net_interface_down(&device->interface);
            break;

        case NET_DEVICE_REQUEST_INTERFACE_ID:
            ret = request_interface_id(device, _out, _out_size);
            break;

        case NET_DEVICE_REQUEST_ADD_ADDR:
            ret = request_add_addr(device, in, in_size);
            break;

        case NET_DEVICE_REQUEST_REMOVE_ADDR:
            ret = request_remove_addr(device, in, in_size);
            break;

        default: {
            ret = STATUS_INVALID_REQUEST;
            break;
        }
    }

    return ret;
}

static const device_ops_t net_device_ops = {
    .type    = FILE_TYPE_CHAR,

    .destroy = net_device_destroy_impl,
    .request = net_device_request,
};

static status_t create_net_device(
    net_device_t *device, const char *name, device_t *parent, module_t *module)
{
    memset(device, 0, sizeof(*device));

    net_interface_init(&device->interface);

    return device_class_create_device(
        &net_device_class, module, name, parent, &net_device_ops, device,
        NULL, 0, 0, &device->node);
}

/**
 * Initializes a new network device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * net_device_publish().
 *
 * @param device        Device to initialize.
 * @param name          Name to give the device node.
 * @param parent        Parent device node.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t net_device_create_etc(net_device_t *device, const char *name, device_t *parent) {
    module_t *module = module_caller();
    return create_net_device(device, name, parent, module);
}

/**
 * Initializes a new network device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * net_device_publish().
 *
 * The device will be named after the module creating the device.
 *
 * @param device        Device to initialize.
 * @param parent        Parent device node (e.g. bus device).
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t net_device_create(net_device_t *device, device_t *parent) {
    module_t *module = module_caller();
    return create_net_device(device, module->name, parent, module);
}

/**
 * Publishes a network device. This completes initialization after the driver
 * has finished initialization, and then publishes the device for use.
 *
 * @param device        Device to publish.
 */
__export void net_device_publish(net_device_t *device) {
    device_publish(device->node);
}

/** Initialize the network device class. */
void net_device_class_init(void) {
    status_t ret = device_class_init(&net_device_class, NET_DEVICE_CLASS_NAME);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to initialize net device class: %d", ret);
}
