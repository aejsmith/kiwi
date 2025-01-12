/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Input device class interface.
 */

#pragma once

#include <kernel/device/input.h>

#include <device/device.h>

__SYS_EXTERN_C_BEGIN

/** Input device type alias. */
typedef device_t input_device_t;

extern status_t input_device_open(const char *path, uint32_t access, uint32_t flags, input_device_t **_device);
extern status_t input_device_from_handle(handle_t handle, input_device_t **_device);

extern input_device_type_t input_device_type(input_device_t *device);

extern status_t input_device_read_event(input_device_t *device, input_event_t *_event);

__SYS_EXTERN_C_END
