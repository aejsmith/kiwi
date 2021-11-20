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

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

static LIST_DEFINE(socket_families);
static MUTEX_DEFINE(socket_families_lock, 0);

static const file_ops_t socket_file_ops;

/** Look up a socket family (lock must be held). */
static socket_family_t *socket_family_lookup(sa_family_t id) {
    list_foreach(&socket_families, iter) {
        socket_family_t *family = list_entry(iter, socket_family_t, link);

        if (family->id == id)
            return family;
    }

    return NULL;
}

/** Look up and reference a socket family. */
static socket_family_t *socket_family_get(sa_family_t id) {
    MUTEX_SCOPED_LOCK(lock, &socket_families_lock);

    socket_family_t *family = socket_family_lookup(id);
    if (family)
        refcount_inc(&family->count);

    return family;
}

/** Release a socket family. */
static void socket_family_release(socket_family_t *family) {
    refcount_dec(&family->count);
}

/** Registers a set of socket families.
 * @param families      Families to unregister.
 * @param ops           Operations for sockets using the family.
 * @return              Status code describing result of the operation. */
status_t socket_families_register(socket_family_t *families, size_t count) {
    MUTEX_SCOPED_LOCK(lock, &socket_families_lock);

    for (size_t i = 0; i < count; i++) {
        if (socket_family_lookup(families[i].id))
            return STATUS_ALREADY_EXISTS;
    }

    for (size_t i = 0; i < count; i++) {
        families[i].file.type = FILE_TYPE_SOCKET;
        families[i].file.ops  = &socket_file_ops;

        refcount_set(&families[i].count, 0);
        list_init(&families[i].link);

        list_append(&socket_families, &families[i].link);
    }

    return STATUS_SUCCESS;
}

/** Unregisters a socket family.
 * @param family        Family to unregister. */
status_t socket_families_unregister(socket_family_t *families, size_t count) {
    MUTEX_SCOPED_LOCK(lock, &socket_families_lock);

    for (size_t i = 0; i < count; i++) {
        if (families[i].count > 0)
            return STATUS_IN_USE;
    }

    for (size_t i = 0; i < count; i++)
        list_remove(&families[i].link);

    return STATUS_SUCCESS;
}

static void socket_file_close(file_handle_t *handle) {
    socket_family_t *family = handle->socket;

    family->ops->close(handle);
    socket_family_release(family);
}

static const file_ops_t socket_file_ops = {
    .close = socket_file_close,
};

status_t socket_accept(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len, object_handle_t **_accepted)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_bind(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_connect(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_getpeername(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_getsockname(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_listen(object_handle_t *handle, int backlog) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_recvfrom(
    object_handle_t *handle, void *buf, size_t size, int flags,
    socklen_t max_addr_len, size_t *_bytes, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_sendto(
    object_handle_t *handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_getsockopt(
    object_handle_t *handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_setsockopt(
    object_handle_t *handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_shutdown(object_handle_t *handle, int how) {
    return STATUS_NOT_IMPLEMENTED;
}

status_t socket_sockatmark(object_handle_t *handle, bool *_mark) {
    return STATUS_NOT_IMPLEMENTED;
}

/** Creates a new socket.
 * @param family        Address family of the socket (AF_*).
 * @param type          Type of the socket (SOCK_*).
 * @param protocol      Protocol number specific to the address family, or 0 to
 *                      use a default protocol for the address family.
 * @param flags         Behaviour flags for the file handle (FILE_*).
 * @param _handle       Where to store created handle.
 * @return              Status code describing result of the operation. */
status_t socket_create(
    sa_family_t family, int type, int protocol, uint32_t flags,
    object_handle_t **_handle)
{
    status_t ret;

    assert(_handle);

    socket_family_t *file = socket_family_get(family);
    if (!file)
        return STATUS_ADDR_NOT_SUPPORTED;

    file_handle_t *handle = file_handle_alloc(&file->file, FILE_ACCESS_READ | FILE_ACCESS_WRITE, flags);

    ret = file->ops->create(handle, family, type, protocol);
    if (ret != STATUS_SUCCESS) {
        file_handle_free(handle);
        socket_family_release(file);
        return ret;
    }

    *_handle = file_handle_create(handle);
    return STATUS_SUCCESS;
}

/** Creates a new pair of connected sockets.
 * @param family        Address family of the socket (AF_*).
 * @param type          Type of the socket (SOCK_*).
 * @param protocol      Protocol number specific to the address family, or 0 to
 *                      use a default protocol for the address family.
 * @param flags         Behaviour flags for the file handle (FILE_*).
 * @param _handles      Where to store created handles.
 * @return              Status code describing result of the operation. */
status_t socket_create_pair(
    sa_family_t family, int type, int protocol, uint32_t flags,
    object_handle_t *_handles[2])
{
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * System calls.
 */

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

/** Creates a new socket.
 * @param family        Address family of the socket (AF_*).
 * @param type          Type of the socket (SOCK_*).
 * @param protocol      Protocol number specific to the address family, or 0 to
 *                      use a default protocol for the address family.
 * @param flags         Behaviour flags for the file handle (FILE_*).
 * @param _handle       Where to store created handle.
 * @return              Status code describing result of the operation. */
status_t kern_socket_create(
    sa_family_t family, int type, int protocol, uint32_t flags,
    handle_t *_handle)
{
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    object_handle_t *handle;
    ret = socket_create(family, type, protocol, flags, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = object_handle_attach(handle, NULL, _handle);
    object_handle_release(handle);
    return ret;
}

/** Creates a new pair of connected sockets.
 * @param family        Address family of the socket (AF_*).
 * @param type          Type of the socket (SOCK_*).
 * @param protocol      Protocol number specific to the address family, or 0 to
 *                      use a default protocol for the address family.
 * @param flags         Behaviour flags for the file handle (FILE_*).
 * @param _handles      Where to store created handles.
 * @return              Status code describing result of the operation. */
status_t kern_socket_create_pair(
    sa_family_t family, int type, int protocol, uint32_t flags,
    handle_t _handles[2])
{
    status_t ret;

    if (!_handles)
        return STATUS_INVALID_ARG;

    object_handle_t *handles[2];
    ret = socket_create_pair(family, type, protocol, flags, handles);
    if (ret != STATUS_SUCCESS)
        return ret;

    handle_t uhandles[2];
    ret = object_handle_attach(handles[0], &uhandles[0], &_handles[0]);
    if (ret == STATUS_SUCCESS) {
        ret = object_handle_attach(handles[1], &uhandles[1], &_handles[1]);
        if (ret != STATUS_SUCCESS)
            object_handle_detach(uhandles[0]);
    }

    object_handle_release(handles[0]);
    object_handle_release(handles[1]);

    return ret;
}
