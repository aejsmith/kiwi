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
 * @brief		Process management functions.
 */

#ifndef __KERNEL_PROCESS_H
#define __KERNEL_PROCESS_H

#include <kernel/object.h>
#include <kernel/security.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Arguments passed to the userspace loader. */
typedef struct process_args {
	char *path;			/**< Path to program. */
	char **args;			/**< Argument array. */
	char **env;			/**< Environment variable array. */
	int args_count;			/**< Number of entries in argument array (excluding NULL). */
	int env_count;			/**< Number of entries in environment array (excluding NULL). */
	void *load_base;		/**< Base address that kernel library was loaded to. */
} process_args_t;

/** Expected path to libkernel. */
#define LIBKERNEL_PATH		"/system/libraries/libkernel.so"

/** Process access rights. */
#define PROCESS_QUERY		(1<<8)	/**< Query process information. */
#define PROCESS_SET_SECURITY	(1<<9)	/**< Set security context. */

/** Process object events. */
#define PROCESS_EVENT_DEATH	0	/**< Wait for process death. */

/** Process creation flags. */
#define PROCESS_CREATE_SESSION	(1<<1)	/**< Place the process in a new session. */

extern status_t kern_process_create(const char *path, const char *const args[],
                                    const char *const env[], int flags,
                                    const security_context_t *sectx,
                                    handle_t map[][2], int count,
                                    const object_security_t *security,
                                    object_rights_t rights, handle_t *handlep);
extern status_t kern_process_replace(const char *path, const char *const args[],
                                     const char *const env[], const security_context_t *sectx,
                                     handle_t map[][2], int count);
extern status_t kern_process_clone(void (*func)(void *), void *arg, void *sp,
                                   const object_security_t *security,
                                   object_rights_t rights, handle_t *handlep);
extern status_t kern_process_open(process_id_t id, object_rights_t rights, handle_t *handlep);
extern process_id_t kern_process_id(handle_t handle);
extern session_id_t kern_process_session(handle_t handle);
extern status_t kern_process_security_context(handle_t handle, security_context_t *contextp);
extern status_t kern_process_set_security_context(handle_t handle, const security_context_t *context);
extern status_t kern_process_status(handle_t handle, int *statusp);
extern void kern_process_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PROCESS_H */
