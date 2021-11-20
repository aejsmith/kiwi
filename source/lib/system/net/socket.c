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
 * @brief               Core socket API.
 */

#include <kernel/status.h>

#include <sys/socket.h>

#include <errno.h>

#include "libsystem.h"

/** Accept a new connection on a socket. */
int accept(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len) {
    socklen_t max_len = (addr) ? *addr_len : 0;

    handle_t accepted;
    status_t ret = kern_socket_accept(socket, max_len, addr, addr_len, &accepted);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (int)accepted;
}

/** Bind a name to a socket. */
int bind(int socket, const struct sockaddr *addr, socklen_t addr_len) {
    status_t ret = kern_socket_bind(socket, addr, addr_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Initiate a connection on a socket. */
int connect(int socket, const struct sockaddr *addr, socklen_t addr_len) {
    status_t ret = kern_socket_connect(socket, addr, addr_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Get the name of the peer socket. */
int getpeername(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len) {
    status_t ret = kern_socket_getpeername(socket, *addr_len, addr, addr_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Get the name of the socket. */
int getsockname(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len) {
    status_t ret = kern_socket_getsockname(socket, *addr_len, addr, addr_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Get the socket options. */
int getsockopt(
    int socket, int level, int opt_name, void *__restrict opt_value,
    socklen_t *__restrict opt_len)
{
    status_t ret = kern_socket_getsockopt(socket, level, opt_name, *opt_len, opt_value, opt_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Listen for socket connections. */
int listen(int socket, int backlog) {
    status_t ret = kern_socket_listen(socket, backlog);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Receive a message from a connected socket. */
ssize_t recv(int socket, void *buf, size_t length, int flags) {
    size_t bytes = 0;
    status_t ret = kern_socket_recvfrom(socket, buf, length, flags, 0, &bytes, NULL, NULL);
    if (ret != STATUS_SUCCESS && bytes == 0) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (ssize_t)bytes;
}

/** Receive a message from a socket. */
ssize_t recvfrom(
    int socket, void *__restrict buf, size_t length, int flags,
    struct sockaddr *__restrict addr, socklen_t *__restrict addr_len)
{
    socklen_t max_addr_len = (addr) ? *addr_len : 0;

    size_t bytes = 0;
    status_t ret = kern_socket_recvfrom(socket, buf, length, flags, max_addr_len, &bytes, addr, addr_len);
    if (ret != STATUS_SUCCESS && bytes == 0) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (ssize_t)bytes;
}

/** Send a message on a socket. */
ssize_t send(int socket, const void *buf, size_t length, int flags) {
    size_t bytes = 0;
    status_t ret = kern_socket_sendto(socket, buf, length, flags, NULL, 0, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (ssize_t)bytes;
}

/** Send a message on a socket. */
ssize_t sendto(
    int socket, const void *buf, size_t length, int flags,
    const struct sockaddr *addr, socklen_t addr_len)
{
    size_t bytes = 0;
    status_t ret = kern_socket_sendto(socket, buf, length, flags, addr, addr_len, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (ssize_t)bytes;
}

/** Set the socket options. */
int setsockopt(
    int socket, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    status_t ret = kern_socket_setsockopt(socket, level, opt_name, opt_value, opt_len);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Shut down socket send and receive operations. */
int shutdown(int socket, int how) {
    status_t ret = kern_socket_shutdown(socket, how);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Determine whether a socket is at the out-of-band mark. */
int sockatmark(int socket) {
    bool mark = false;
    status_t ret = kern_socket_sockatmark(socket, &mark);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return (int)mark;
}

/** Create an endpoint for communication. */
int socket(int domain, int type, int protocol) {
    int kern_type = type & __SOCK_TYPE;

    uint32_t flags = 0;
    if (type & SOCK_NONBLOCK)
        flags |= FILE_NONBLOCK;

    handle_t handle;
    status_t ret = kern_socket_create(domain, kern_type, protocol, flags, &handle);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    /* Mark the handle as inheritable if not opening with SOCK_CLOEXEC. */
    if (!(type & SOCK_CLOEXEC))
        kern_handle_set_flags(handle, HANDLE_INHERITABLE);

    return (int)handle;
}

/** Create a pair of connected sockets. */
int socketpair(int domain, int type, int protocol, int sockets[2]) {
    int kern_type = type & __SOCK_TYPE;

    uint32_t flags = 0;
    if (type & SOCK_NONBLOCK)
        flags |= FILE_NONBLOCK;

    handle_t handles[2];
    status_t ret = kern_socket_create_pair(domain, kern_type, protocol, flags, handles);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    /* Mark the handles as inheritable if not opening with SOCK_CLOEXEC. */
    if (!(type & SOCK_CLOEXEC)) {
        kern_handle_set_flags(handles[0], HANDLE_INHERITABLE);
        kern_handle_set_flags(handles[1], HANDLE_INHERITABLE);
    }

    sockets[0] = (int)handles[0];
    sockets[1] = (int)handles[1];

    return 0;
}
