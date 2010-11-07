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
 * @brief		POSIX internal functions/definitions.
 */

#ifndef __POSIX_PRIV_H
#define __POSIX_PRIV_H

#include <util/list.h>
#include <util/mutex.h>

#include <unistd.h>

#include "../libc.h"

/** Structure containing details of a POSIX process. */
typedef struct posix_process {
	list_t header;			/**< Link to process list. */
	handle_t handle;		/**< Handle to process. */
	pid_t pid;			/**< ID of the process. */
} posix_process_t;

extern list_t __hidden child_processes;
extern libc_mutex_t __hidden child_processes_lock;

#endif /* __POSIX_PRIV_H */
