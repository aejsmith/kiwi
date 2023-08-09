/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Test IPC service.
 */

#include <core/log.h>
#include <core/service.h>

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol.h"

int main(int argc, char **argv) {
    status_t ret;

    handle_t port;
    ret = kern_port_create(&port);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to create port: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    ret = core_service_register_port(port);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to register port: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    core_log(CORE_LOG_NOTICE, "server started and registered");

    handle_t handle;
    ret = kern_port_listen(port, -1, &handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to listen for connection: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    handle_t process;
    ret = kern_connection_open_remote(handle, &process);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to open remote: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    process_id_t pid;
    ret = kern_process_id(process, &pid);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to get remote PID: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    kern_handle_close(process);

    core_log(CORE_LOG_NOTICE, "server got connection from PID %" PRId32, pid);

    core_connection_t *conn = core_connection_create(handle, CORE_CONNECTION_RECEIVE_REQUESTS);
    if (!conn)
        return EXIT_FAILURE;

    core_message_t *signal = core_message_create_signal(TEST_SIGNAL_START, 0, 0);
    ret = core_connection_signal(conn, signal);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "server failed to send signal: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    core_message_destroy(signal);

    while (true) {
        core_message_t *request;

        ret = core_connection_receive(conn, -1, &request);
        if (ret != STATUS_SUCCESS) {
            if (ret == STATUS_CONN_HUNGUP) {
                break;
            } else {
                core_log(CORE_LOG_ERROR, "server failed to receive message: %" PRId32, ret);
                return EXIT_FAILURE;
            }
        }

        core_message_type_t type = core_message_type(request);
        uint32_t id              = core_message_id(request);
        size_t size              = core_message_size(request);
        nstime_t timestamp       = core_message_timestamp(request);

        if (type != CORE_MESSAGE_REQUEST || id != TEST_REQUEST_PING || size != sizeof(test_request_ping_t)) {
            core_log(CORE_LOG_ERROR, "server received invalid message");
            return EXIT_FAILURE;
        }

        test_request_ping_t *ping = (test_request_ping_t *)core_message_data(request);
        ping->string[sizeof(ping->string) - 1] = 0;

        core_log(CORE_LOG_NOTICE, "server received: %u '%s' (timestamp: %" PRIu64 ")", ping->index, ping->string, timestamp);

        core_message_t *reply = core_message_create_reply(request, sizeof(test_request_ping_t), 0);

        test_request_ping_t *pong = (test_request_ping_t *)core_message_data(reply);
        pong->index = ping->index;
        snprintf(pong->string, sizeof(pong->string), "PONG %" PRIu32, ping->index);

        ret = core_connection_reply(conn, reply);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "server failed to send reply: %" PRId32, ret);
            return EXIT_FAILURE;
        }

        core_message_destroy(reply);
        core_message_destroy(request);
    }

    core_connection_close(conn);

    return EXIT_SUCCESS;
}
