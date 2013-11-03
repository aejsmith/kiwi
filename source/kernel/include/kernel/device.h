/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Device functions.
 */

#ifndef __KERNEL_DEVICE_H
#define __KERNEL_DEVICE_H

#include <kernel/file.h>
#include <kernel/limits.h>

#ifdef __cplusplus
extern "C" {
#endif

extern status_t kern_device_open(const char *path, uint32_t rights,
	uint32_t flags, handle_t *handlep);

extern status_t kern_device_request(handle_t handle, unsigned request,
	const void *in, size_t in_size, void *out, size_t out_size,
	size_t *bytesp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_DEVICE_H */
