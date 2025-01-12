/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
