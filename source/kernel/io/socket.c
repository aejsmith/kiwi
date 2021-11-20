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

#include <io/socket.h>

#include <status.h>

status_t kern_socket_accept(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len, handle_t *_accepted)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_bind(handle_t handle, const sockaddr_t *addr, socklen_t addr_len) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_connect(handle_t handle, const sockaddr_t *addr, socklen_t addr_len) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_getpeername(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_getsockname(
    handle_t handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_listen(handle_t handle, int backlog) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_recvfrom(
    handle_t handle, void *buf, size_t size, int flags, socklen_t max_addr_len,
    size_t *_bytes, sockaddr_t *_addr, socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_sendto(
    handle_t handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_getsockopt(
    handle_t handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_setsockopt(
    handle_t handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_shutdown(handle_t handle, int how) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_sockatmark(handle_t handle, bool *_mark) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_create(int domain, int type, int protocol, handle_t *_handle) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t kern_socket_create_pair(int domain, int type, int protocol, handle_t _handles[2]) {
    return STATUS_NOT_IMPLEMENTED;
}
