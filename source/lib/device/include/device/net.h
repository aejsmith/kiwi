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
 * @brief               Network device class interface.
 */

#pragma once

#include <kernel/device/net.h>

#include <device/device.h>

__SYS_EXTERN_C_BEGIN

/** Network device type alias. */
typedef device_t net_device_t;

extern status_t net_device_open(const char *path, uint32_t access, uint32_t flags, net_device_t **_device);
extern status_t net_device_from_handle(handle_t handle, net_device_t **_device);

extern status_t net_device_up(net_device_t *device);
extern status_t net_device_down(net_device_t *device);
extern status_t net_device_interface_id(net_device_t *device, uint32_t *_interface_id);
extern status_t net_device_hw_addr(net_device_t *device, uint8_t *_hw_addr, size_t *_hw_addr_len);
extern status_t net_device_add_addr(net_device_t *device, const void *addr, size_t size);
extern status_t net_device_remove_addr(net_device_t *device, const void *addr, size_t size);

__SYS_EXTERN_C_END
