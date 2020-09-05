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
 * @brief               IPC service functions.
 */

#include <core/mutex.h>
#include <core/service.h>

#include <kernel/ipc.h>
#include <kernel/status.h>

#include "../../../services/service_manager/protocol.h"

static CORE_MUTEX_DEFINE(service_lock);
static handle_t service_manager_conn = INVALID_HANDLE;

/**
 * Looks up the service with the given name in the current process' service
 * manager, and opens a connection to it.
 *
 * @param name          Service name.
 * @param service_flags Flags influencing service lookup behaviour
 *                      (CORE_SERVICE_*).
 * @param conn_flags    Flags for the connection object (CORE_CONNECTION_*).
 * @param _conn         Where to return connection object upon success.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NOT_FOUND if service is not found.
 *                      Any possible status code from core_connection_open().
 */
status_t core_service_connect(
    const char *name, uint32_t service_flags, uint32_t conn_flags,
    core_connection_t **_conn)
{
    status_t ret;

    core_mutex_lock(&service_lock, -1);

    if (service_manager_conn == INVALID_HANDLE) {
        ret = kern_connection_open(PROCESS_ROOT_PORT, -1, &service_manager_conn);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = STATUS_NOT_IMPLEMENTED;

out:
    core_mutex_unlock(&service_lock);
    return ret;
}
