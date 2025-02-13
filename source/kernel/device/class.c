/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device class management.
 */

#include <device/class.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

/** Initialises a device class.
 * @param class         Class to initialise.
 * @param name          Name of the class.
 * @return              Status code describing the result of the operation. */
status_t device_class_init(device_class_t *class, const char *name) {
    class->name    = name;
    class->next_id = 0;

    status_t ret = device_create_etc(module_caller(), name, device_class_dir, NULL, NULL, NULL, 0, &class->dir);
    if (ret != STATUS_SUCCESS)
        return ret;

    device_publish(class->dir);

    return STATUS_SUCCESS;
}

/** Destroys a device class.
 * @param class         Class to destroy.
 * @return              Status code describing the result of the operation. */
status_t device_class_destroy(device_class_t *class) {
    return device_destroy(class->dir);
}

/**
 * Creates a device belonging to a device class. The specified name and parent
 * location should be the physical location of the device in the device tree
 * (e.g. under its bus/controller). This function will handle creation of an
 * alias for the device under the class' alias tree.
 *
 * The supplied attributes should not contain the "class" attribute - it will
 * be added by this function.
 *
 * @see                 device_create().
 *
 * @param class         Class to create device under.
 * @param flags         Behaviour flags (DEVICE_CLASS_CREATE_DEVICE_*).
 */
status_t device_class_create_device(
    device_class_t *class, module_t *module, const char *name, device_t *parent,
    const device_ops_t *ops, void *data, device_attr_t *attrs, size_t count,
    uint32_t flags, device_t **_device)
{
    status_t ret;

    size_t new_attr_count = count + 1;
    device_attr_t *new_attrs = kmalloc(sizeof(*new_attrs) * new_attr_count, MM_KERNEL);

    if (count > 0)
        memcpy(&new_attrs[1], attrs, sizeof(*new_attrs) * count);

    new_attrs[0].name         = DEVICE_ATTR_CLASS;
    new_attrs[0].type         = DEVICE_ATTR_STRING;
    new_attrs[0].value.string = class->name;

    device_t *device;
    ret = device_create_etc(module, name, parent, ops, data, new_attrs, new_attr_count, &device);

    kfree(new_attrs);

    if (ret != STATUS_SUCCESS)
        return ret;

    if (!(flags & DEVICE_CLASS_CREATE_DEVICE_NO_ALIAS)) {
        // TODO: ID reuse. Can use device resource management to release IDs.
        uint32_t id = atomic_fetch_add(&class->next_id, 1);

        /* This should always succeed as the ID/name should be unique. */
        char alias[16];
        sprintf(alias, "%" PRId32, id);
        ret = device_alias_etc(module_caller(), alias, class->dir, device, NULL);
        assert(ret == STATUS_SUCCESS);
    }

    if (_device)
        *_device = device;

    return STATUS_SUCCESS;
}
