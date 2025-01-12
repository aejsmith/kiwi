/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
