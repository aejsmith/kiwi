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
 * @brief               DT bus manager internal definitions.
 */

#pragma once

#include <device/bus/dt.h>

typedef void (*dt_iterate_cb_t)(dt_device_t *device, void *data);

extern void dt_iterate(dt_iterate_cb_t cb, void *data);

extern dt_driver_t *dt_match_builtin_driver(dt_device_t *device, builtin_dt_driver_type_t type);

static inline void dt_device_unmatch(dt_device_t *device) {
    device->driver = NULL;
    device->match  = NULL;
    device->flags &= DT_DEVICE_MATCHED;
}
