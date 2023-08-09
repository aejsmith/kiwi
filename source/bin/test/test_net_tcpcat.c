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
 * @brief               Test TCP netcat-like application.
 */

#include <arpa/inet.h>

#include <sys/socket.h>

#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 1024

static int connect_host(
    const char *host, const char *service, int type,
    struct sockaddr_storage *_addr, socklen_t *_addr_len)
{
    struct addrinfo hints = {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = type;

    struct addrinfo *result;
    int ret = getaddrinfo(host, service, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    int sock_fd;

    for (struct addrinfo *addr = result; addr; addr = addr->ai_next) {
        sock_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock_fd < 0) {
            if (addr->ai_next)
                continue;

            perror("socket");
            goto out;
        }

        if (connect(sock_fd, addr->ai_addr, addr->ai_addrlen) < 0) {
            close(sock_fd);
            sock_fd = -1;

            if (addr->ai_next)
                continue;

            perror("connect");
            goto out;
        }

        memcpy(_addr, addr->ai_addr, addr->ai_addrlen);
        *_addr_len = addr->ai_addrlen;
        break;
    }

out:
    freeaddrinfo(result);
    return sock_fd;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_storage addr_storage;
    socklen_t addr_len;

    int sock_fd = connect_host(argv[1], argv[2], SOCK_STREAM, &addr_storage, &addr_len);
    if (sock_fd < 0)
        return EXIT_FAILURE;

    char addr_str[INET6_ADDRSTRLEN] = {};
    uint16_t port = 0;
    if (addr_storage.ss_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&addr_storage;
        inet_ntop(AF_INET, &addr->sin_addr, addr_str, sizeof(addr_str));
        port = ntohs(addr->sin_port);
    } else if (addr_storage.ss_family == AF_INET6) {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&addr_storage;
        inet_ntop(AF_INET6, &addr->sin6_addr, addr_str, sizeof(addr_str));
        port = ntohs(addr->sin6_port);
    }

    printf("Connected to %s:%u\n", addr_str, port);

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
                } else if (read_bytes == 0) {
                    return EXIT_SUCCESS;
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
}
