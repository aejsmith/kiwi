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
 * @brief               Test IPC client.
 */

#include <core/log.h>
#include <core/service.h>
#include <core/time.h>

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../services/test/protocol.h"

#define TEST_PING_COUNT 15

int main(int argc, char **argv) {
    status_t ret;

    core_connection_t *conn;
    ret = core_service_open("org.kiwi.test", 0, CORE_CONNECTION_RECEIVE_SIGNALS, &conn);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Client failed to open connection: %d\n", ret);
        return EXIT_FAILURE;
    }

    /* Wait until told to start. */
    core_message_t *signal;
    ret = core_connection_receive(conn, -1, &signal);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Client failed to receive message: %d\n", ret);
        return EXIT_FAILURE;
    }

    core_message_type_t type = core_message_type(signal);
    uint32_t id              = core_message_id(signal);

    if (type != CORE_MESSAGE_SIGNAL || id != TEST_SIGNAL_START) {
        fprintf(stderr, "Client received invalid message\n");
        return EXIT_FAILURE;
    }

    core_message_destroy(signal);

    printf("Client received start signal\n");

    unsigned int count = 0;
    while (count < TEST_PING_COUNT) {
        core_message_t *request = core_message_create_request(TEST_REQUEST_PING, sizeof(test_request_ping_t), 0);

        test_request_ping_t *ping = (test_request_ping_t *)core_message_data(request);
        ping->index = count;
        snprintf(ping->string, sizeof(ping->string), "PING %" PRIu32, ping->index);

        core_message_t *reply;
        ret = core_connection_request(conn, request, &reply);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Client failed to send request: %d\n", ret);
            return EXIT_FAILURE;
        }

        type               = core_message_type(reply);
        id                 = core_message_id(reply);
        size_t size        = core_message_size(reply);
        nstime_t timestamp = core_message_timestamp(reply);

        if (type != CORE_MESSAGE_REPLY || id != TEST_REQUEST_PING || size != sizeof(test_request_ping_t)) {
            fprintf(stderr, "Client received invalid message\n");
            return EXIT_FAILURE;
        }

        test_request_ping_t *pong = (test_request_ping_t *)core_message_data(reply);
        pong->string[sizeof(pong->string) - 1] = 0;

        printf("Client received: %" PRIu32 " '%s' (timestamp: %" PRIu64 ")\n", pong->index, pong->string, timestamp);

        core_message_destroy(request);
        core_message_destroy(reply);

        if (++count != TEST_PING_COUNT)
            kern_thread_sleep(core_msecs_to_nsecs(500), NULL);
    }

    return EXIT_SUCCESS;
}
