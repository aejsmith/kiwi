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
 * @brief               IPC test application.
 */

#include <core/ipc.h>
#include <core/time.h>

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SIGNAL_START       1
#define TEST_REQUEST_PING       2

#define TEST_PING_COUNT         15

#define TEST_STRING_LEN         16

extern char **environ;

typedef struct test_request_ping {
    uint32_t index;
    char string[TEST_STRING_LEN];
} test_request_ping_t;

static bool spawn_client(handle_t port) {
    process_attrib_t attrib;
    process_attrib_init(&attrib);
    attrib.root_port = port;

    const char *args[] = { "/system/bin/test_ipc", "--client", NULL };
    status_t ret = kern_process_create(args[0], args, (const char *const *)environ, 0, &attrib, NULL);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create process: %" PRId32 "\n", ret);
        return false;
    }

    return true;
}

static int test_server(void) {
    status_t ret;

    handle_t port;
    ret = kern_port_create(&port);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create port: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    printf("Created port (handle: %" PRId32 ")\n", port);

    if (!spawn_client(port))
        return EXIT_FAILURE;

    handle_t handle;
    ret = kern_port_listen(port, -1, &handle);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Server failed to listen for connection: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    handle_t process;
    ret = kern_connection_open_remote(handle, &process);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Server failed to open remote: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    process_id_t pid;
    ret = kern_process_id(process, &pid);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Server failed to get remote PID: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    kern_handle_close(process);

    printf("Server got connection (handle: %" PRId32 ")\n", handle);
    printf("Client PID: %" PRId32 "\n", pid);

    core_connection_t *conn = core_connection_create(handle, CORE_CONNECTION_RECEIVE_REQUESTS);
    if (!conn)
        return EXIT_FAILURE;

    core_message_t *signal = core_message_create_signal(TEST_SIGNAL_START, 0, 0);
    ret = core_connection_signal(conn, signal);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Server failed to send signal: %" PRId32 "\n", ret);
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
                fprintf(stderr, "Server failed to receive message: %" PRId32 "\n", ret);
                return EXIT_FAILURE;
            }
        }

        core_message_type_t type = core_message_type(request);
        uint32_t id              = core_message_id(request);
        size_t size              = core_message_size(request);
        nstime_t timestamp       = core_message_timestamp(request);

        if (type != CORE_MESSAGE_REQUEST || id != TEST_REQUEST_PING || size != sizeof(test_request_ping_t)) {
            fprintf(stderr, "Server received invalid message\n");
            return EXIT_FAILURE;
        }

        test_request_ping_t *ping = (test_request_ping_t *)core_message_data(request);
        ping->string[sizeof(ping->string) - 1] = 0;

        printf("Server received: %u '%s' (timestamp: %" PRIu64 ")\n", ping->index, ping->string, timestamp);

        core_message_t *reply = core_message_create_reply(request, sizeof(test_request_ping_t), 0);

        test_request_ping_t *pong = (test_request_ping_t *)core_message_data(reply);
        pong->index = ping->index;
        snprintf(pong->string, sizeof(pong->string), "PONG %" PRIu32, ping->index);

        ret = core_connection_reply(conn, reply);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Server failed to send reply: %" PRId32 "\n", ret);
            return EXIT_FAILURE;
        }

        core_message_destroy(reply);
        core_message_destroy(request);
    }

    core_connection_close(conn);

    return EXIT_SUCCESS;
}

static int test_client(void) {
    status_t ret;

    core_connection_t *conn;
    ret = core_connection_open(PROCESS_ROOT_PORT, -1, CORE_CONNECTION_RECEIVE_SIGNALS, &conn);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Client failed to open connection: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    /* Wait until told to start. */
    core_message_t *signal;
    ret = core_connection_receive(conn, -1, &signal);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Client failed to receive message: %" PRId32 "\n", ret);
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
            fprintf(stderr, "Client failed to send request: %" PRId32 "\n", ret);
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

        printf("Client received: %u '%s' (timestamp: %" PRIu64 ")\n", pong->index, pong->string, timestamp);

        core_message_destroy(request);
        core_message_destroy(reply);

        if (++count != TEST_PING_COUNT)
            kern_thread_sleep(core_msecs_to_nsecs(500), NULL);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--client") == 0) {
        return test_client();
    } else {
        return test_server();
    }
}
