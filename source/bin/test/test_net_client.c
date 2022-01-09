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
 * @brief               Test socket client.
 */

#include <arpa/inet.h>

#include <sys/socket.h>

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

static int tcp_client(struct sockaddr_in *addr) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)addr, sizeof(*addr)) != 0) {
        perror("connect");
        return EXIT_FAILURE;
    }

    for (size_t count = 0; count < 10; count++) {
        char msg[MESSAGE_MAX + 1];
        ssize_t size = snprintf(msg, MESSAGE_MAX, "PING %zu", count);
        msg[size++] = 0;

        ssize_t sent = send(fd, msg, size, 0);
        if (sent < 0) {
            perror("send");
            return EXIT_FAILURE;
        }

        printf("Client sent %ld of %ld bytes\n", sent, size);

        size = recv(fd, msg, MESSAGE_MAX, 0);
        if (size < 0) {
            perror("recv");
            return EXIT_FAILURE;
        } else if (size == 0) {
            printf("Client shutdown\n");
            return EXIT_SUCCESS;
        }

        msg[size] = 0;

        printf("Client received %ld bytes '%s'\n", size, msg);

        sleep(1);
    }

    return EXIT_SUCCESS;
}

static int udp_client(struct sockaddr_in *addr) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    for (size_t count = 0; count < 10; count++) {
        char msg[MESSAGE_MAX + 1];
        ssize_t size = snprintf(msg, MESSAGE_MAX, "PING %zu", count);
        msg[size] = 0;

        ssize_t sent = sendto(fd, msg, size, 0, (struct sockaddr *)addr, sizeof(*addr));
        if (sent < 0) {
            perror("sendto");
            return EXIT_FAILURE;
        }

        printf("Client sent %ld of %ld bytes\n", sent, size);

        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        size = recvfrom(fd, msg, MESSAGE_MAX, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (size < 0) {
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        msg[size] = 0;

        printf(
            "Client received %ld byte message '%s' from %u.%u.%u.%u\n",
            size, msg, IP_PRINT_ARGS(&recv_addr));

        sleep(1);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 3) {
        struct sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(TEST_PORT);

        // TODO
        //if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) {
        if (!parse_ipv4_addr(argv[2], &addr.sin_addr.s_addr)) {
            printf("Invalid IP address\n");
            return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "-t") == 0) {
            return tcp_client(&addr);
        } else if (strcmp(argv[1], "-u") == 0) {
            return udp_client(&addr);
        }
    }

    printf("Usage: %s [-t|-u] <server IP>\n", argv[0]);
    return EXIT_FAILURE;
}
