/*
 * Copyright (C) 2009-2022 Alex Smith
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

#define __NEED_struct_timespec
#define __NEED_time_t
#include <bits/alltypes.h>

#include <core/ipc.h>
#include <core/list.h>
#include <core/mutex.h>

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#include "libsystem.h"

__SYS_EXTERN_C_BEGIN

struct environ;

extern core_connection_t *posix_service_get(void) __sys_hidden;
extern void posix_service_put(void) __sys_hidden;

static inline bool posix_request_failed(int32_t ret) {
    libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
    libsystem_status_to_errno(ret);
    return false;
}

/**
 * Processes.
 */

/** Structure containing details of a POSIX process. */
typedef struct posix_process {
    core_list_t header;             /**< Link to process list. */
    handle_t handle;                /**< Handle to process. */
    pid_t pid;                      /**< ID of the process. */
} posix_process_t;

extern core_list_t __sys_hidden child_processes;
extern core_mutex_t __sys_hidden child_processes_lock;

extern void posix_register_fork_handler(void (*func)(void)) __sys_hidden;

extern int posix_get_terminal(uint32_t access, uint32_t flags, handle_t *_handle) __sys_hidden;

/**
 * Filesystem.
 */

extern mode_t __sys_hidden current_umask;

extern void posix_fs_exec(struct environ *env) __sys_hidden;

/**
 * Signals.
 */

extern void posix_signal_exec(struct environ *env) __sys_hidden;

extern void posix_signal_guard_begin(void) __sys_hidden;
extern void posix_signal_guard_end(void) __sys_hidden;

static inline void posix_signal_guard_endp(void *p) {
    posix_signal_guard_end();
}

/** RAII-style scoped signal guard. */
#define POSIX_SCOPED_SIGNAL_GUARD(name) \
    posix_signal_guard_begin(); \
    uint32_t name __sys_unused __sys_cleanup(posix_signal_guard_endp) = 0;

extern int posix_signal_from_exception(unsigned code) __sys_hidden;

extern void sigsetjmp_save(sigjmp_buf env, int save_mask) __sys_hidden;
extern void siglongjmp_restore(sigjmp_buf env) __sys_hidden;

/**
 * Time.
 */

static inline nstime_t nstime_from_timespec(const struct timespec *tp) {
    return ((nstime_t)tp->tv_sec * 1000000000) + tp->tv_nsec;
}

static inline void nstime_to_timespec(nstime_t time, struct timespec *tp) {
    tp->tv_nsec = time % 1000000000;
    tp->tv_sec  = time / 1000000000;
}

/**
 * Exported POSIX functions, used as implementation details (e.g. by the
 * terminal service).
 */

extern pid_t posix_get_pgrp_session(pid_t pgid);
extern int posix_set_session_terminal(pid_t sid, handle_t handle);

__SYS_EXTERN_C_END
