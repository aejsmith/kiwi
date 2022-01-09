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
 * @brief               Test socket server.
 */

#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/wait.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_PORT 12345
#define MESSAGE_MAX 128

// TODO: Replace with inet_ntop
typedef union {
    uint32_t addr;
    uint8_t bytes[4];
} ip_union_t;
#define IP_PRINT_ARGS(a) \
    ((ip_union_t *)&(a)->sin_addr.s_addr)->bytes[0], \
    ((ip_union_t *)&(a)->sin_addr.s_addr)->bytes[1], \
    ((ip_union_t *)&(a)->sin_addr.s_addr)->bytes[2], \
    ((ip_union_t *)&(a)->sin_addr.s_addr)->bytes[3]

static void tcp_conn(int fd, const struct sockaddr_in *addr) {
    char msg[MESSAGE_MAX + 1];
    size_t msg_size = 0;

    size_t msg_count = 0;
    size_t recv_count = 0;

    while (true) {
        char buf[MESSAGE_MAX];
        ssize_t size = recv(fd, buf, MESSAGE_MAX, 0);
        if (size < 0) {
            perror("recv");
            return;
        } else if (size == 0) {
            printf("Server shutdown from %u.%u.%u.%u\n", IP_PRINT_ARGS(addr));
            return;
        }

        recv_count++;

        for (ssize_t i = 0; i < size; i++) {
            if (buf[i] == 0 || msg_size == MESSAGE_MAX) {
                msg[msg_size] = 0;
                printf(
                    "Server received %lu byte (%lu receives) message '%s' from %u.%u.%u.%u\n",
                    msg_size, recv_count, msg, IP_PRINT_ARGS(addr));

                recv_count = 0;

                msg_size = snprintf(msg, MESSAGE_MAX, "PONG %zu", msg_count);
                msg[msg_size++] = 0;
                ssize_t sent = send(fd, msg, msg_size, 0);
                if (sent < 0) {
                    perror("send");
                    return;
                }

                printf("Server sent %ld of %ld bytes\n", sent, msg_size);

                msg_count++;
                msg_size = 0;
            } else {
                msg[msg_size++] = buf[i];
            }
        }
    }
}

static int tcp_server(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(fd, 5) != 0) {
        perror("listen");
        return EXIT_FAILURE;
    }

    while (true) {
        /* Clean up children. */
        waitpid(-1, NULL, WNOHANG);

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int conn_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept");
            return EXIT_FAILURE;
        }

        printf(
            "Server connection from %u.%u.%u.%u\n",
            IP_PRINT_ARGS(&client_addr));

        int pid = fork();
        if (pid == 0) {
            close(fd);
            tcp_conn(conn_fd, &client_addr);
            return EXIT_SUCCESS;
        } else if (pid == -1) {
            perror("fork");
            return EXIT_FAILURE;
        }
    }
}

static int udp_server(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    for (size_t count = 0; ; count++) {
        char msg[MESSAGE_MAX + 1];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        ssize_t size = recvfrom(fd, msg, MESSAGE_MAX, 0, (struct sockaddr *)&client_addr, &client_len);
        if (size < 0) {
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        msg[size] = 0;

        printf(
            "Server received %ld byte message '%s' from %u.%u.%u.%u\n",
            size, msg, IP_PRINT_ARGS(&client_addr));

        size = snprintf(msg, MESSAGE_MAX, "PONG %zu", count);
        msg[size] = 0;

        ssize_t sent = sendto(fd, msg, size, 0, (struct sockaddr *)&client_addr, client_len);
        if (sent < 0) {
            perror("sendto");
            return EXIT_FAILURE;
        }

        printf("Server sent %ld of %ld bytes\n", sent, size);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "-t") == 0) {
            return tcp_server();
        } else if (strcmp(argv[1], "-u") == 0) {
            return udp_server();
        }
    }

    fprintf(stderr, "Usage: %s [-t|-u]\n", argv[0]);
    return EXIT_FAILURE;
}
