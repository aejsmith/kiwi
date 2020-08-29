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
 * @brief               IPC test application.
 */

#include <kernel/ipc.h>
#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MESSAGE_PING       1
#define TEST_MESSAGE_PONG       2

#define TEST_DATA_LEN           128

#define TEST_PING_COUNT         10

extern char **environ;

static void dump_message(ipc_message_t *msg) {
    int i;

    printf(" flags     = 0x%x\n", msg->flags);
    printf(" id        = %u\n", msg->id);
    printf(" size      = %u\n", msg->size);
    printf(" timestamp = %llu\n", msg->timestamp);
    for (i = 0; i < 6; i++)
        printf(" args[%d]   = 0x%lx\n", i, msg->args[i]);
}

static bool spawn_client(handle_t port) {
    const char *args[] = { "/system/bin/test-ipc", "--client", NULL };
    handle_t map[3][2] = { { 0, 0 }, { 1, 1 }, { 2, 2 } };
    process_attrib_t attrib;
    status_t ret;

    attrib.token = INVALID_HANDLE;
    attrib.root_port = port;
    attrib.map = map;
    attrib.map_count = 3;

    ret = kern_process_create(args[0], args, (const char *const *)environ, 0, &attrib, NULL);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create process: %d\n", ret);
        return false;
    }

    return true;
}

static int test_server(void) {
    ipc_message_t msg;
    ipc_client_t client;
    char data[TEST_DATA_LEN];
    handle_t port, conn;
    status_t ret;

    ret = kern_port_create(&port);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create port: %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("Created port (handle: %d)\n", port);

    if (!spawn_client(port))
        return EXIT_FAILURE;

    ret = kern_port_listen(port, &client, -1, &conn);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to listen for connection: %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("Server got connection (handle: %d)\n", conn);
    printf("Client PID: %d\n", client.pid);

    while (true) {
        ret = kern_connection_receive(conn, &msg, NULL, -1);
        if (ret != STATUS_SUCCESS) {
            if (ret != STATUS_CONN_HUNGUP)
                fprintf(stderr, "Server failed to receive message: %d\n", ret);

            return EXIT_FAILURE;
        }

        if (msg.id != TEST_MESSAGE_PING || msg.size != sizeof(data)) {
            fprintf(stderr, "Server received invalid message\n");
            return EXIT_FAILURE;
        }

        ret = kern_connection_receive_data(conn, data);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Server failed to receive data: %d\n", ret);
            return EXIT_FAILURE;
        }

        data[sizeof(data) - 1] = 0;
        printf("%s\n", data);
        dump_message(&msg);

        snprintf(data, sizeof(data), "PONG %lu", msg.args[0]);
        msg.id = TEST_MESSAGE_PONG;
        msg.size = sizeof(data);

        ret = kern_connection_send(conn, &msg, data, INVALID_HANDLE, -1);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Server failed to send message: %d\n", ret);
            return EXIT_FAILURE;
        }
    }
}

static int test_client(void) {
    ipc_message_t msg;
    unsigned long count = 1;
    char data[TEST_DATA_LEN];
    handle_t conn;
    status_t ret;

    ret = kern_connection_open(PROCESS_ROOT_PORT, -1, &conn);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to open connection: %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("Client got connection (handle: %d)\n", conn);

    while (count <= TEST_PING_COUNT) {
        snprintf(data, sizeof(data), "PING %lu", count);
        msg.id = TEST_MESSAGE_PING;
        msg.size = sizeof(data);
        msg.args[0] = count;
        msg.args[1] = 0xdeadcafe;
        msg.args[2] = 0xdeadc0de;
        msg.args[3] = 0xcafebeef;
        msg.args[4] = 0xcafebabe;
        msg.args[5] = 0x1337cafe;

        ret = kern_connection_send(conn, &msg, data, INVALID_HANDLE, -1);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Client failed to send message: %d\n", ret);
            return EXIT_FAILURE;
        }

        ret = kern_connection_receive(conn, &msg, NULL, -1);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Client failed to receive message: %d\n", ret);
            return EXIT_FAILURE;
        }

        if (msg.id != TEST_MESSAGE_PONG || msg.size != sizeof(data)) {
            fprintf(stderr, "Client received invalid message\n");
            return EXIT_FAILURE;
        } else if (msg.args[0] != count) {
            fprintf(stderr, "Client received message with incorrect count\n");
            return EXIT_FAILURE;
        }

        ret = kern_connection_receive_data(conn, data);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Client failed to receive data: %d\n", ret);
            return EXIT_FAILURE;
        }

        data[sizeof(data) - 1] = 0;
        printf("%s\n", data);
        dump_message(&msg);

        if (count++ != TEST_PING_COUNT)
            kern_thread_sleep(1000000000, NULL);
    }

    return EXIT_FAILURE;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--client") == 0) {
        return test_client();
    } else {
        return test_server();
    }
}
