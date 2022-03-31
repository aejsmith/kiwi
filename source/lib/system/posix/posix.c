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
 * @brief               POSIX internal functions.
 */

#include <core/service.h>

#include <kernel/status.h>
#include <kernel/thread.h>

#include <services/posix_service.h>

#include <signal.h>

#include "posix/posix.h"

static CORE_MUTEX_DEFINE(posix_service_lock);
static core_connection_t *posix_service_conn = NULL;

static void posix_service_fork(void) {
    /* Connections are not inheritable. */
    if (posix_service_conn) {
        core_connection_destroy(posix_service_conn);
        posix_service_conn = NULL;
    }
}

static __sys_init_prio(LIBSYSTEM_INIT_PRIO_POSIX_SERVICE) void posix_service_init(void) {
    posix_register_fork_handler(posix_service_fork);
}

/**
 * Takes the POSIX service lock and gets the connection to it, opening it if it
 * is not already open. Call posix_service_unlock() when done with it.
 *
 * On success, this begins a signal guard, which ends when posix_service_put()
 * is called. This is necessary to prevent deadlock because signal handling
 * also needs to use the POSIX service.
 *
 * @return              POSIX service connection, or NULL on error.
 */
core_connection_t *posix_service_get(void) {
    /* Raise the IPL before taking the lock to ensure signals will not be
     * received. */
    posix_signal_guard_begin();

    core_mutex_lock(&posix_service_lock, -1);

    if (!posix_service_conn) {
        status_t ret = core_service_open(POSIX_SERVICE_NAME, 0, 0, &posix_service_conn);
        if (ret != STATUS_SUCCESS) {
            libsystem_log(CORE_LOG_WARN, "failed to connect to POSIX service: %" PRId32, ret);

            core_mutex_unlock(&posix_service_lock);
            posix_signal_guard_end();
            return NULL;
        }
    }

    return posix_service_conn;
}

/** Releases the POSIX service lock and end signal guard. */
void posix_service_put(void) {
    core_mutex_unlock(&posix_service_lock);
    posix_signal_guard_end();
}
