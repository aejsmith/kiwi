/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               DT bus manager internal definitions.
 */

#pragma once

#include <device/bus/dt.h>

typedef void (*dt_iterate_cb_t)(dt_device_t *device, void *data);

extern bool dt_device_match(dt_device_t *device, dt_driver_t *driver, dt_match_t *match);
extern void dt_device_unmatch(dt_device_t *device);

extern const char *dt_get_builtin_driver_name(dt_driver_t *driver);
extern dt_driver_t *dt_match_builtin_driver(dt_device_t *device, builtin_dt_driver_type_t type);

extern void dt_iterate(dt_iterate_cb_t cb, void *data);

extern bool dt_irq_init_device(dt_device_t *device);
extern void dt_irq_deinit_device(dt_device_t *device);
