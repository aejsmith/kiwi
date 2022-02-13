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

#include <services/posix_service.h>

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

static __sys_init void posix_service_init(void) {
    register_fork_handler(posix_service_fork);
}

/**
 * Takes the POSIX service lock and gets the connection to it, opening it if it
 * is not already open. Call posix_service_unlock() when done with it.
 *
 * @return              POSIX service connection, or NULL on error.
 */
core_connection_t *posix_service_get(void) {
    core_mutex_lock(&posix_service_lock, -1);

    if (!posix_service_conn) {
        status_t ret = core_service_connect(POSIX_SERVICE_NAME, 0, 0, &posix_service_conn);
        if (ret != STATUS_SUCCESS) {
            libsystem_log(CORE_LOG_WARN, "failed to connect to POSIX service: %" PRId32);
            core_mutex_unlock(&posix_service_lock);
            return NULL;
        }
    }

    return posix_service_conn;
}

/** Releases the POSIX service lock. */
void posix_service_put(void) {
    core_mutex_unlock(&posix_service_lock);
}
