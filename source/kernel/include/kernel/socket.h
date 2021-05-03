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
 * @brief               Socket API.
 */

#pragma once

#include <kernel/file.h>

__KERNEL_EXTERN_C_BEGIN

/**
 * POSIX standard definitions.
 */

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

typedef struct sockaddr {
    sa_family_t sa_family;
    char sa_data[];
} sockaddr_t;

#define SOCKADDR_STORAGE_SIZE   128

typedef struct sockaddr_storage {
    sa_family_t ss_family;
    char sa_data[SOCKADDR_STORAGE_SIZE - sizeof(sa_family_t)];
} __kernel_aligned(sizeof(void *)) sockaddr_storage_t;

#define SOCK_DGRAM              1
#define SOCK_RAW                2
#define SOCK_SEQPACKET          3
#define SOCK_STREAM             4

#define SOL_SOCKET              1

#define SO_ACCEPTCONN           1
#define SO_BROADCAST            2
#define SO_DEBUG                3
#define SO_DONTROUTE            4
#define SO_ERROR                5
#define SO_KEEPALIVE            6
#define SO_LINGER               7
#define SO_OOBINLINE            8
#define SO_RCVBUF               9
#define SO_RCVLOWAT             10
#define SO_RCVTIMEO             11
#define SO_REUSEADDR            12
#define SO_SNDBUF               13
#define SO_SNDLOWAT             14
#define SO_SNDTIMEO             15
#define SO_TYPE                 16

#define SOMAXCONN               4096

#define MSG_CTRUNC              (1<<0)
#define MSG_DONTROUTE           (1<<1)
#define MSG_EOR                 (1<<2)
#define MSG_OOB                 (1<<3)
#define MSG_NOSIGNAL            (1<<4)
#define MSG_PEEK                (1<<5)
#define MSG_TRUNC               (1<<6)
#define MSG_WAITALL             (1<<7)

#define AF_UNSPEC               0
#define AF_INET                 1
#define AF_INET6                2
#define AF_UNIX                 3

#define SHUT_RD                 0
#define SHUT_RDWR               1
#define SHUT_WR                 2

/**
 * Kernel API.
 */

extern status_t kern_socket_accept(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len, handle_t *_accepted);
extern status_t kern_socket_bind(handle_t handle, const sockaddr_t *addr, socklen_t addr_len);
extern status_t kern_socket_connect(handle_t handle, const sockaddr_t *addr, socklen_t addr_len);
extern status_t kern_socket_getpeername(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len);
extern status_t kern_socket_getsockname(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len);
extern status_t kern_socket_listen(handle_t handle, int backlog);
extern status_t kern_socket_recv(handle_t handle, void *buf, size_t size, int flags, size_t *_bytes);
extern status_t kern_socket_recvfrom(
    handle_t handle, void *buf, size_t size, int flags, socklen_t max_addr_len,
    size_t *_bytes, sockaddr_t *_addr, socklen_t *_addr_len);
extern status_t kern_socket_send(handle_t handle, const void *buf, size_t size, int flags, size_t *_bytes);
extern status_t kern_socket_sendto(
    handle_t handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes);
extern status_t kern_socket_getsockopt(
    handle_t handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len);
extern status_t kern_socket_setsockopt(
    handle_t handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len);
extern status_t kern_socket_shutdown(handle_t handle, int how);
extern status_t kern_socket_sockatmark(handle_t handle, bool *_mark);

extern status_t kern_socket_create(int domain, int type, int protocol, handle_t *_handle);
extern status_t kern_socket_create_pair(int domain, int type, int protocol, handle_t _handles[2]);

__KERNEL_EXTERN_C_END
