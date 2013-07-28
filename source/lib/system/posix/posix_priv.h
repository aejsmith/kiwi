/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX internal functions/definitions.
 */

#ifndef __POSIX_PRIV_H
#define __POSIX_PRIV_H

#include <kernel/object.h>

#include <util/list.h>
//#include <util/mutex.h>

#include <unistd.h>

#include "libsystem.h"

/** Structure containing details of a POSIX process. */
typedef struct posix_process {
	list_t header;			/**< Link to process list. */
	handle_t handle;		/**< Handle to process. */
	pid_t pid;			/**< ID of the process. */
} posix_process_t;

extern list_t __hidden child_processes;
//extern libc_mutex_t __hidden child_processes_lock;

extern mode_t __hidden current_umask;

//extern object_acl_t *posix_mode_to_acl(object_acl_t *current, mode_t mode) __hidden;

#endif /* __POSIX_PRIV_H */
