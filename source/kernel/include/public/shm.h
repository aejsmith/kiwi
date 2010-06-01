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
 * @brief		Shared memory functions.
 */

#ifndef __KERNEL_SHM_H
#define __KERNEL_SHM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

extern handle_t SYSCALL(shm_create)(size_t size);
extern handle_t SYSCALL(shm_open)(shm_id_t id);
extern shm_id_t SYSCALL(shm_id)(handle_t handle);
extern int SYSCALL(shm_resize)(handle_t handle, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SHM_H */
