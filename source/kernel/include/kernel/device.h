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
 * @brief               Device functions.
 */

#pragma once

#include <kernel/file.h>
#include <kernel/limits.h>

__KERNEL_EXTERN_C_BEGIN

/** Start of class-specific event/request numbers. */
#define DEVICE_CLASS_EVENT_START    32
#define DEVICE_CLASS_REQUEST_START  32

/** Start of device-specific event/request numbers. */
#define DEVICE_CUSTOM_EVENT_START   1024
#define DEVICE_CUSTOM_REQUEST_START 1024

extern status_t kern_device_open(const char *path, uint32_t access, uint32_t flags, handle_t *_handle);

__KERNEL_EXTERN_C_END
