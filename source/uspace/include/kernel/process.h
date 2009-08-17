/* Kiwi process functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Process functions.
 */

#ifndef __KERNEL_PROCESS_H
#define __KERNEL_PROCESS_H

#include <kernel/types.h>

/** Process arguments structure. */
typedef struct process_args {
	char **args;			/**< Argument array (path to program should be first entry). */
	char **env;			/**< Environment variable array. */
	int args_count;			/**< Number of entries in argument array (excluding NULL-terminator). */
	int env_count;			/**< Number of entries in environment array (excluding NULL-terminator). */
} process_args_t;

extern handle_t process_create(char *const args[], char *const environ[], bool inherit);
extern int process_replace(char *const args[], char *const environ[], bool inherit);
extern int process_duplicate(handle_t *handlep);
extern handle_t process_open(identifier_t id);
extern identifier_t process_id(handle_t handle);
extern int process_exit(int status);

#endif /* __KERNEL_PROCESS_H */
