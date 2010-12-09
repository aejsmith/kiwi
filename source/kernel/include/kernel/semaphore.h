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
 * @brief		Semaphore functions.
 */

#ifndef __KERNEL_SEMAPHORE_H
#define __KERNEL_SEMAPHORE_H

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Semaphore access rights. */
#define SEMAPHORE_USAGE		(1<<8)	/**< Allow usage of the semaphore. */

/** Maximum length of a semaphore name. */
#define SEMAPHORE_NAME_MAX	32

extern status_t kern_semaphore_create(const char *name, size_t count,
                                      const object_security_t *security,
                                      object_rights_t rights, handle_t *handlep);
extern status_t kern_semaphore_open(semaphore_id_t id, object_rights_t rights, handle_t *handlep);
extern semaphore_id_t kern_semaphore_id(handle_t handle);
extern status_t kern_semaphore_down(handle_t handle, useconds_t timeout);
extern status_t kern_semaphore_up(handle_t handle, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SEMAPHORE_H */
