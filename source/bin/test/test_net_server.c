/*
 * Copyright (C) 2009-2021 Alex Smith
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_PORT 12345
#define MESSAGE_MAX 128

int main(int argc, char **argv) {
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

        printf("Server received %ld byte message '%s'\n", size, msg);

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
