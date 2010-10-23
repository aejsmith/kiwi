/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Memory area functions.
 */

#ifndef __KERNEL_AREA_H
#define __KERNEL_AREA_H

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Access rights for area objects. */
#define AREA_READ	(1<<8)		/**< Allow mapping for reading. */
#define AREA_WRITE	(1<<9)		/**< Allow mapping for writing. */

extern status_t SYSCALL(area_create)(size_t size, handle_t source, offset_t offset,
                                     const object_security_t *security,
                                     object_rights_t rights, handle_t *handlep);
extern status_t SYSCALL(area_open)(area_id_t id, object_rights_t rights, handle_t *handlep);
extern area_id_t SYSCALL(area_id)(handle_t handle);
extern size_t SYSCALL(area_size)(handle_t handle);
extern status_t SYSCALL(area_resize)(handle_t handle, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_AREA_H */
