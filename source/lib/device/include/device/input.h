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
