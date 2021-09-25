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
 * @brief               Device class management.
 *
 * These are some helpers for implementing device classes. They take care of
 * maintaining the class's aliases in the device tree (/class/<name>/...).
 */

#pragma once

#include <device/device.h>

/** Device class structure. */
typedef struct device_class {
    const char *name;                   /**< Name of the class. */
    device_t *dir;                      /**< Class directory. */
    atomic_int32_t next_id;             /**< Next device ID. */
} device_class_t;

extern status_t device_class_init(device_class_t *class, const char *name);
extern status_t device_class_destroy(device_class_t *class);

/** Flags for device_class_create_device(). */
enum {
    /**
     * Do not create a class alias device. Intended for use where a device is
     * a child of another device of the same class, e.g. partitions.
     */
    DEVICE_CLASS_CREATE_DEVICE_NO_ALIAS = (1<<0),
};

extern status_t device_class_create_device(
    device_class_t *class, module_t *module, const char *name, device_t *parent,
    device_ops_t *ops, void *data, device_attr_t *attrs, size_t count,
    uint32_t flags, device_t **_device);
