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

#include <io/file.h>

#include <lib/list.h>
#include <lib/refcount.h>

#include <kernel/socket.h>

struct socket_family;

/** Socket operations structure. */
typedef struct socket_ops {
    /** Creates a socket.
     * @param handle        File handle structure.
     * @param family        Address family of the socket.
     * @param type          Type of the socket.
     * @param protocol      Protocol number.
     * @return              Status code describing the result of the operation. */
    status_t (*create)(file_handle_t *handle, sa_family_t family, int type, int protocol);

    /** Closes the socket.
     * @param handle        File handle structure. */
    void (*close)(file_handle_t *handle);
} socket_ops_t;

/** Structure describing a supported socket family. */
typedef struct socket_family {
    /**
     * File header (all sockets on a family share the same file since actual
     * state is stored in per-handle data).
     */
    file_t file;

    list_t link;                    /**< Link to families list. */
    refcount_t count;               /**< Sockets open using the family. */
    sa_family_t id;                 /**< Family ID (AF_*). */
    const socket_ops_t *ops;        /**< Operations implementing the family. */
} socket_family_t;

extern status_t socket_families_register(socket_family_t *families, size_t count);
extern status_t socket_families_unregister(socket_family_t *families, size_t count);

extern status_t socket_accept(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len, object_handle_t **_accepted);
extern status_t socket_bind(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len);
extern status_t socket_connect(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len);
extern status_t socket_getpeername(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len);
extern status_t socket_getsockname(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len);
extern status_t socket_listen(object_handle_t *handle, int backlog);
extern status_t socket_recvfrom(
    object_handle_t *handle, void *buf, size_t size, int flags,
    socklen_t max_addr_len, size_t *_bytes, sockaddr_t *_addr,
    socklen_t *_addr_len);
extern status_t socket_sendto(
    object_handle_t *handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes);
extern status_t socket_getsockopt(
    object_handle_t *handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len);
extern status_t socket_setsockopt(
    object_handle_t *handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len);
extern status_t socket_shutdown(object_handle_t *handle, int how);
extern status_t socket_sockatmark(object_handle_t *handle, bool *_mark);

extern status_t socket_create(
    sa_family_t family, int type, int protocol, uint32_t flags,
    object_handle_t **_handle);
extern status_t socket_create_pair(
    sa_family_t family, int type, int protocol, uint32_t flags,
    object_handle_t *_handles[2]);
