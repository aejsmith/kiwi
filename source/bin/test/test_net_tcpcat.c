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
 * @brief               Test TCP netcat-like application.
 */

#include <arpa/inet.h>

#include <sys/socket.h>

#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024

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

// TODO: Replace with inet_pton
static bool parse_ipv4_addr(const char *str, uint32_t *addr) {
    uint32_t vals[4];
    int pos = 0;
    int ret = sscanf(str, "%u.%u.%u.%u%n", &vals[0], &vals[1], &vals[2], &vals[3], &pos);
    if (ret != 4 || vals[0] > 255 || vals[1] > 255 || vals[2] > 255 || vals[3] > 255 || str[pos] != 0) {
        fprintf(stderr, "net_control: invalid address '%s'\n", str);
        return false;
    }

    ip_union_t a;
    a.bytes[0] = vals[0];
    a.bytes[1] = vals[1];
    a.bytes[2] = vals[2];
    a.bytes[3] = vals[3];

    *addr = a.addr;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtol(argv[2], NULL, 10);

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);

    // TODO
    //if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
    if (!parse_ipv4_addr(argv[1], &addr.sin_addr.s_addr)) {
        printf("Invalid IP address\n");
        return EXIT_FAILURE;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("connect");
        return EXIT_FAILURE;
    }

    printf("Connected to %u.%u.%u.%u:%u\n", IP_PRINT_ARGS(&addr), port);

    struct pollfd poll_fds[2];
    poll_fds[0].fd     = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd     = sock_fd;
    poll_fds[1].events = POLLIN;

    while (true) {
        if (poll(poll_fds, 2, -1) == -1) {
            perror("poll");
            return EXIT_FAILURE;
        }

        for (int i = 0; i < 2; i++) {
            if (poll_fds[i].revents & POLLIN) {
                char buf[BUF_SIZE];
                ssize_t read_bytes = read(poll_fds[i].fd, buf, sizeof(buf));
                if (read_bytes < 0) {
                    perror("read");
                    return EXIT_FAILURE;
                }

                int out_fd = (poll_fds[i].fd == 0) ? sock_fd : 1;

                char *data = buf;
                while (read_bytes > 0) {
                    ssize_t write_bytes = write(out_fd, data, read_bytes);
                    if (write_bytes < 0) {
                        perror("write");
                        return EXIT_FAILURE;
                    }

                    data += write_bytes;
                    read_bytes -= write_bytes;
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
