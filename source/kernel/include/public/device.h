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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

/** Arguments for device_request(). */
typedef struct device_request_args {
	handle_id_t handle;		/**< Handle to device. */
	int request;			/**< Request number. */
	void *in;			/**< Input buffer. */
	size_t insz;			/**< Input buffer size. */
	void *out;			/**< Output buffer. */
	size_t outsz;			/**< Output buffer size. */
	size_t *bytesp;			/**< Where to store number of bytes returned. */
} device_request_args_t;

/** Generic device events. */
#define DEVICE_EVENT_READABLE		0	/**< Wait for the device to be readable. */
#define DEVICE_EVENT_WRITABLE		1	/**< Wait for the device to be writable. */
//#define DEVICE_EVENT_CHILD_ADDED	2	/**< Wait for a child device to be added. */
//#define DEVICE_EVENT_CHILD_REMOVED	3	/**< Wait for a child device to be removed. */

extern handle_id_t SYSCALL(device_open)(const char *path);
extern int SYSCALL(device_read)(handle_id_t handle, void *buf, size_t count, offset_t offset,
                                size_t *bytesp);
extern int SYSCALL(device_write)(handle_id_t handle, const void *buf, size_t count,
                                 offset_t offset, size_t *bytesp);
#ifdef KERNEL
extern int SYSCALL(device_request)(device_request_args_t *args);
#else
extern int SYSCALL(_device_request)(device_request_args_t *args);
extern int SYSCALL(device_request)(handle_id_t handle, int request, void *in, size_t insz,
                                   void *out, size_t outsz, size_t *bytesp);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_DEVICE_H */
