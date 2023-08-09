/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Input device class.
 */

#pragma once

#include <device/device.h>

#include <kernel/device/input.h>

#define INPUT_MODULE_NAME "input"

/** Input device structure. */
typedef struct input_device {
    device_t *node;                     /**< Device tree node. */

    input_device_type_t type;           /**< Type of the device. */

    mutex_t clients_lock;               /**< Lock for clients list. */
    list_t clients;                     /**< List of clients. */
} input_device_t;

extern void input_device_event(input_device_t *device, input_event_t *event);

/** Destroys an input device.
 * @see                 device_destroy().
 * @param device        Device to destroy. */
static inline status_t input_device_destroy(input_device_t *device) {
    return device_destroy(device->node);
}

extern status_t input_device_create_etc(
    input_device_t *device, const char *name, device_t *parent,
    input_device_type_t type);
extern status_t input_device_create(input_device_t *device, device_t *parent, input_device_type_t type);
extern void input_device_publish(input_device_t *device);
