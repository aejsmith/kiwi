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
 * @brief               Input device class manager.
 *
 * TODO:
 *  - Proper handling for dropping events when the queue is full. This is
 *    necessary for where the client might be tracking state from events which
 *    give relative state (e.g. tracking button state). See how Linux libevdev
 *    handles this.
 */

#include <device/input.h>

#include <module.h>
#include <status.h>

// handle nonblock
// event queue per client

static device_t *input_class_dir;

static status_t input_init(void) {
    status_t ret = device_create_dir(INPUT_DEVICE_CLASS_NAME, device_class_dir, &input_class_dir);
    if (ret != STATUS_SUCCESS)
        return ret;

    return STATUS_SUCCESS;
}

static status_t input_unload(void) {
    return device_destroy(input_class_dir);
}

MODULE_NAME(INPUT_MODULE_NAME);
MODULE_DESC("Input device class manager");
MODULE_FUNCS(input_init, input_unload);
