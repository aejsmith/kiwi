/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Various device manager limitations. */
#define DEVICE_PATH_MAX			256	/**< Maximum length of a device tree path. */

/** Device rights. */
#define DEVICE_RIGHT_QUERY		(1<<0)	/**< Query device information. */
#define DEVICE_RIGHT_READ		(1<<1)	/**< Read directly from the device. */
#define DEVICE_RIGHT_WRITE		(1<<2)	/**< Write directly to the device. */

/** Generic device events. */
#define DEVICE_EVENT_READABLE		0	/**< Wait for the device to be readable. */
#define DEVICE_EVENT_WRITABLE		1	/**< Wait for the device to be writable. */
//#define DEVICE_EVENT_CHILD_ADDED	2	/**< Wait for a child device to be added. */
//#define DEVICE_EVENT_CHILD_REMOVED	3	/**< Wait for a child device to be removed. */

extern status_t kern_device_open(const char *path, object_rights_t rights, handle_t *handlep);
extern status_t kern_device_read(handle_t handle, void *buf, size_t count, offset_t offset,
                                 size_t *bytesp);
extern status_t kern_device_write(handle_t handle, const void *buf, size_t count,
                                  offset_t offset, size_t *bytesp);
extern status_t kern_device_request(handle_t handle, int request, const void *in, size_t insz,
                                    void *out, size_t outsz, size_t *bytesp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_DEVICE_H */
