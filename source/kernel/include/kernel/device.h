/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
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
#define DEVICE_PATH_MAX			4096	/**< Maximum length of a device tree path. */

/** Device rights. */
#define DEVICE_QUERY			(1<<8)	/**< Query device information. */
#define DEVICE_READ			(1<<9)	/**< Read directly from the device. */
#define DEVICE_WRITE			(1<<10)	/**< Write directly to the device. */

/** Generic device events. */
#define DEVICE_EVENT_READABLE		0	/**< Wait for the device to be readable. */
#define DEVICE_EVENT_WRITABLE		1	/**< Wait for the device to be writable. */
//#define DEVICE_EVENT_CHILD_ADDED	2	/**< Wait for a child device to be added. */
//#define DEVICE_EVENT_CHILD_REMOVED	3	/**< Wait for a child device to be removed. */

extern status_t SYSCALL(device_open)(const char *path, object_rights_t rights, handle_t *handlep);
extern status_t SYSCALL(device_read)(handle_t handle, void *buf, size_t count, offset_t offset,
                                     size_t *bytesp);
extern status_t SYSCALL(device_write)(handle_t handle, const void *buf, size_t count,
                                      offset_t offset, size_t *bytesp);
extern status_t SYSCALL(device_request)(handle_t handle, int request, void *in, size_t insz,
                                        void *out, size_t outsz, size_t *bytesp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_DEVICE_H */
