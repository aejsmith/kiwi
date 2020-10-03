/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               POSIX internal functions/definitions.
 */

#pragma once

#include <core/list.h>
#include <core/mutex.h>

#include <kernel/object.h>

#include <unistd.h>

#include "libsystem.h"

/** Structure containing details of a POSIX process. */
typedef struct posix_process {
    core_list_t header;             /**< Link to process list. */
    handle_t handle;                /**< Handle to process. */
    pid_t pid;                      /**< ID of the process. */
} posix_process_t;

extern core_list_t __sys_hidden child_processes;
extern core_mutex_t __sys_hidden child_processes_lock;

extern mode_t __sys_hidden current_umask;

extern void register_fork_handler(void (*func)(void)) __sys_hidden;
