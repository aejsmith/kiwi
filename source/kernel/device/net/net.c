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
 * @brief               Network device class.
 */

#include <device/net/net.h>

#include <device/class.h>

#include <lib/string.h>

#include <module.h>
#include <status.h>

static device_class_t net_device_class;

/** Clean up all data associated with a device.
 * @param device        Device to destroy. */
static void net_device_destroy_impl(device_t *node) {
    net_device_t *device = node->private;

    // TODO. Call device destroy function.
    (void)device;
    fatal("TODO");
}

static device_ops_t net_device_ops = {
    .type    = FILE_TYPE_CHAR,

    .destroy = net_device_destroy_impl,
    // TODO.
};

static status_t create_net_device(
    net_device_t *device, const char *name, device_t *parent, module_t *module)
{
    memset(device, 0, sizeof(*device));

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
