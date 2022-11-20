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
 * @brief               IPC service functions.
 */

#include <core/mutex.h>
#include <core/service.h>

#include <kernel/ipc.h>
#include <kernel/status.h>

#include <services/service_manager.h>

#include <string.h>

#include "posix/posix.h"

static CORE_MUTEX_DEFINE(service_lock);
static core_connection_t *service_manager_conn = NULL;

static void core_service_fork(void) {
    /* Clear the service manager connection - connections are not inheritable. */
    if (service_manager_conn) {
        core_connection_destroy(service_manager_conn);
        service_manager_conn = NULL;
    }
}

static __sys_init_prio(LIBSYSTEM_INIT_PRIO_CORE_SERVICE) void core_service_init(void) {
    posix_register_fork_handler(core_service_fork);
}

static status_t open_service_manager(void) {
    status_t ret = STATUS_SUCCESS;

    if (!service_manager_conn)
        ret = core_connection_open(PROCESS_ROOT_PORT, -1, 0, &service_manager_conn);

    return ret;
}

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
status_t core_service_open(
    const char *name, uint32_t service_flags, uint32_t conn_flags,
    core_connection_t **_conn)
{
    status_t ret;

    CORE_MUTEX_SCOPED_LOCK(lock, &service_lock);

    ret = open_service_manager();
    if (ret != STATUS_SUCCESS)
        return ret;

    size_t name_len = strlen(name);
    libsystem_assert(name_len > 0);
    name_len++;

    core_connection_t *conn;

    /* It is possible for a service to exit in between us receiving its port
     * from the service manager and trying to connect to it. To handle this,
     * we'll loop. */
    ret = STATUS_CONN_HUNGUP;
    while (ret == STATUS_CONN_HUNGUP) {
        core_message_t *request = core_message_create_request(
            SERVICE_MANAGER_REQUEST_CONNECT,
            sizeof(service_manager_request_connect_t) + name_len, 0);
        if (!request) {
            ret = STATUS_NO_MEMORY;
            break;
        }

        service_manager_request_connect_t *request_data =
            (service_manager_request_connect_t *)core_message_data(request);

        request_data->flags = service_flags;
        memcpy(request_data->name, name, name_len);

        core_message_t *reply;
        ret = core_connection_request(service_manager_conn, request, &reply);
        if (ret == STATUS_SUCCESS) {
            libsystem_assert(core_message_size(reply) == sizeof(service_manager_reply_connect_t));

            const service_manager_reply_connect_t *reply_data =
                (const service_manager_reply_connect_t *)core_message_data(reply);

            ret = reply_data->result;

            if (ret == STATUS_SUCCESS) {
                handle_t port = core_message_detach_handle(reply);

                libsystem_assert(port != INVALID_HANDLE);

                ret = core_connection_open(port, -1, conn_flags, &conn);
                kern_handle_close(port);
            }

            core_message_destroy(reply);
        }

        core_message_destroy(request);
    }

    if (ret == STATUS_SUCCESS)
        *_conn = conn;

    return ret;
}

/**
 * Register an IPC service with the service manager. This is only a valid
 * operation for a service process that has been started by the service manager.
 * It will associate the service's name as configured in the service manager
 * with the given port, and subsequent requests to connect to the service will
 * be directed to the port.
 *
 * @param port          Port to register for the service.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_REQUEST if the calling process is not
 *                      known as an IPC service to the service manager.
 *                      STATUS_ALREADY_EXISTS if a port is already registered
 *                      for the service.
 *                      STATUS_NO_MEMORY if allocation fails.
 *                      Any error returned by core_connection_request().
 */
status_t core_service_register_port(handle_t port) {
    status_t ret;

    CORE_MUTEX_SCOPED_LOCK(lock, &service_lock);

    ret = open_service_manager();
    if (ret != STATUS_SUCCESS)
        return ret;

    core_message_t *request = core_message_create_request(SERVICE_MANAGER_REQUEST_REGISTER_PORT, 0, 0);
    if (!request)
        return STATUS_NO_MEMORY;

    core_message_attach_handle(request, port, false);

    core_message_t *reply;
    ret = core_connection_request(service_manager_conn, request, &reply);
    if (ret == STATUS_SUCCESS) {
        libsystem_assert(core_message_size(reply) == sizeof(service_manager_reply_register_port_t));

        const service_manager_reply_register_port_t *reply_data =
            (const service_manager_reply_register_port_t *)core_message_data(reply);

        ret = reply_data->result;

        core_message_destroy(reply);
    }

    core_message_destroy(request);

    return ret;
}

/**
 * Gets a handle to the process for a running service. If the service is not
 * running, this will fail.
 *
 * @param name          Service name.
 * @param _handle       Where to store handle to process.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NOT_RUNNING if not running.
 *                      Any error returned by core_connection_request().
 */
status_t core_service_get_process(const char *name, handle_t *_handle) {
    status_t ret;

    CORE_MUTEX_SCOPED_LOCK(lock, &service_lock);

    ret = open_service_manager();
    if (ret != STATUS_SUCCESS)
        return ret;

    size_t name_len = strlen(name);
    libsystem_assert(name_len > 0);
    name_len++;

    core_message_t *request = core_message_create_request(
        SERVICE_MANAGER_REQUEST_GET_PROCESS,
        sizeof(service_manager_request_get_process_t) + name_len, 0);
    if (!request)
        return STATUS_NO_MEMORY;

    service_manager_request_get_process_t *request_data =
        (service_manager_request_get_process_t *)core_message_data(request);

    memcpy(request_data->name, name, name_len);

    core_message_t *reply;
    ret = core_connection_request(service_manager_conn, request, &reply);
    if (ret == STATUS_SUCCESS) {
        libsystem_assert(core_message_size(reply) == sizeof(service_manager_reply_get_process_t));

        const service_manager_reply_get_process_t *reply_data =
            (const service_manager_reply_get_process_t *)core_message_data(reply);

        ret = reply_data->result;

        if (ret == STATUS_SUCCESS) {
            handle_t handle = core_message_detach_handle(reply);

            libsystem_assert(handle != INVALID_HANDLE);

            *_handle = handle;
        }

        core_message_destroy(reply);
    }

    core_message_destroy(request);

    return ret;
}
