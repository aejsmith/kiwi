/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               i8042 keyboard/mouse port driver.
 */

#include <device/input.h>

#include <module.h>
#include <status.h>

static device_t *i8042_controller_dir;

static status_t i8042_init(void) {
    status_t ret = device_create_dir("i8042", device_bus_platform_dir, &i8042_controller_dir);
    if (ret != STATUS_SUCCESS)
        return ret;

    return STATUS_SUCCESS;
}

static status_t i8042_unload(void) {
    return device_destroy(i8042_controller_dir);
}

MODULE_NAME("i8042");
MODULE_DESC("i8042 keyboard/mouse port driver");
MODULE_DEPS(INPUT_MODULE_NAME);
MODULE_FUNCS(i8042_init, i8042_unload);
