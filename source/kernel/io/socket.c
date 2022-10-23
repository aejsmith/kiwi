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
 * @brief               Socket API.
 */

#include <io/request.h>
#include <io/socket.h>

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/safe.h>

#include <assert.h>
#include <status.h>

static LIST_DEFINE(socket_families);
static MUTEX_DEFINE(socket_families_lock, 0);

static const file_ops_t socket_file_ops;

/**
 * Maximum option value for *sockopt(). Don't know if this is appropriate for
 * everything, but need to stop users passing in a giant buffer size.
 */
#define SOCKOPT_LEN_MAX     128

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
        family->count++;

    return family;
}

/** Release a socket family. */
static void socket_family_release(sa_family_t id) {
    MUTEX_SCOPED_LOCK(lock, &socket_families_lock);

    socket_family_t *family = socket_family_lookup(id);
    assert(family);
    family->count--;
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
        families[i].count = 0;

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

static status_t socket_do_send(
    file_handle_t *handle, io_request_t *request, int flags,
    const sockaddr_t *addr, socklen_t addr_len)
{
    socket_t *socket = handle->socket;

    return socket->ops->send(socket, request, flags, addr, addr_len);
}

static status_t socket_do_receive(
    file_handle_t *handle, io_request_t *request, int flags,
    socklen_t max_addr_len, sockaddr_t *_addr, socklen_t *_addr_len)
{
    socket_t *socket = handle->socket;

    return socket->ops->receive(socket, request, flags, max_addr_len, _addr, _addr_len);
}

static void socket_file_close(file_handle_t *handle) {
    socket_t *socket = handle->socket;

    /* Save this since close() frees it. */
    sa_family_t family = socket->family;

    socket->ops->close(socket);
    socket_family_release(family);
}

static status_t socket_file_io(file_handle_t *handle, io_request_t *request) {
    if (request->op == IO_OP_WRITE) {
        return socket_do_send(handle, request, 0, NULL, 0);
    } else {
        return socket_do_receive(handle, request, 0, 0, NULL, 0);
    }
}

static status_t socket_file_wait(file_handle_t *handle, object_event_t *event) {
    socket_t *socket = handle->socket;

    if (!socket->ops->wait)
        return STATUS_INVALID_EVENT;

    return socket->ops->wait(socket, event);
}

static void socket_file_unwait(file_handle_t *handle, object_event_t *event) {
    socket_t *socket = handle->socket;

    if (socket->ops->unwait)
        socket->ops->unwait(socket, event);
}

static void socket_file_info(file_handle_t *handle, file_info_t *info) {
    info->block_size = PAGE_SIZE;
    info->size       = 0;
    info->id         = 0;
    info->mount      = 0;
    info->type       = FILE_TYPE_SOCKET;
    info->links      = 1;
    info->created    = 0;
    info->accessed   = 0;
    info->modified   = 0;
}

static const file_ops_t socket_file_ops = {
    .close  = socket_file_close,
    .io     = socket_file_io,
    .wait   = socket_file_wait,
    .unwait = socket_file_unwait,
    .info   = socket_file_info,
};

/** Validate that a handle is a socket and return its file_handle_t if so. */
static file_handle_t *get_socket_handle(object_handle_t *handle) {
    if (handle->type->id != OBJECT_TYPE_FILE)
        return NULL;

    file_handle_t *fhandle = handle->private;

    if (fhandle->file->ops != &socket_file_ops)
        return NULL;

    return fhandle;
}

status_t socket_accept(
    object_handle_t *handle, socklen_t max_len, sockaddr_t *_addr,
    socklen_t *_addr_len, object_handle_t **_accepted)
{
    return STATUS_NOT_IMPLEMENTED;
}

/** Bind a socket to a local address.
 * @param handle        Handle to socket to bind.
 * @param addr          Local address to bind.
 * @param addr_len      Length of local address.
 * @return              Status code describing result of the operation. */
status_t socket_bind(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len) {
    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle)
        return STATUS_INVALID_HANDLE;

    socket_t *socket = fhandle->socket;

    if (!socket->ops->bind)
        return STATUS_NOT_SUPPORTED;

    return socket->ops->bind(socket, addr, addr_len);
}

/** Initiate a connection on a socket.
 * @param handle        Handle to socket to connect.
 * @param addr          Destination address.
 * @param addr_len      Length of destination address.
 * @return              Status code describing result of the operation. */
status_t socket_connect(object_handle_t *handle, const sockaddr_t *addr, socklen_t addr_len) {
    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle)
        return STATUS_INVALID_HANDLE;

    socket_t *socket = fhandle->socket;

    if (!socket->ops->connect)
        return STATUS_NOT_SUPPORTED;

    return socket->ops->connect(socket, addr, addr_len);
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

/** Receives data from a socket.
 * @param handle        Handle to socket to receive from.
 * @param buf           Buffer to receive data into.
 * @param size          Maximum size of data to receive.
 * @param flags         Behaviour flags (MSG_*).
 * @param max_addr_len  Maximum length of returned address (size of buffer).
 * @param _bytes        Where to store number of bytes received.
 * @param _addr         Where to return address of the source (can be NULL).
 * @param _addr_len     Where to return actual size of the source address. Can
 *                      be larger than max_addr_len, in which case the address
 *                      will have been truncated.
 * @return              Status code describing result of the operation. */
status_t socket_recvfrom(
    object_handle_t *handle, void *buf, size_t size, int flags,
    socklen_t max_addr_len, size_t *_bytes, sockaddr_t *_addr,
    socklen_t *_addr_len)
{
    status_t ret;

    assert(handle);
    assert(buf);
    assert(max_addr_len == 0 || (_addr && _addr_len));

    io_request_t request;
    request.transferred = 0;

    if (_addr_len)
        *_addr_len = 0;

    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle) {
        ret = STATUS_INVALID_HANDLE;
        goto out;
    }

    io_vec_t vec;
    vec.buffer = buf;
    vec.size   = size;

    ret = io_request_init(&request, &vec, 1, 0, IO_OP_READ, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        goto out;

    ret = socket_do_receive(
        fhandle, &request, flags, max_addr_len,
        (max_addr_len > 0) ? _addr : NULL, (max_addr_len > 0) ? _addr_len : NULL);

    io_request_destroy(&request);

out:
    if (_bytes)
        *_bytes = request.transferred;

    return ret;
}

/** Sends data on a socket.
 * @param handle        Handle to socket to send on.
 * @param buf           Buffer containing data to send.
 * @param size          Size of data to send.
 * @param flags         Behaviour flags (MSG_*).
 * @param addr          Address of the destination, for connectionless-mode
 *                      sockets (can be NULL). Ignored for connection-mode
 *                      sockets.
 * @param addr_len      Size of the address structure.
 * @param _bytes        Where to store number of bytes sent.
 * @return              Status code describing result of the operation. */
status_t socket_sendto(
    object_handle_t *handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes)
{
    status_t ret;

    assert(handle);
    assert(buf);
    assert(addr_len == 0 || addr);

    io_request_t request;
    request.transferred = 0;

    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle) {
        ret = STATUS_INVALID_HANDLE;
        goto out;
    }

    io_vec_t vec;
    vec.buffer = (void *)buf;
    vec.size   = size;

    ret = io_request_init(&request, &vec, 1, 0, IO_OP_WRITE, IO_TARGET_KERNEL);
    if (ret != STATUS_SUCCESS)
        goto out;

    ret = socket_do_send(fhandle, &request, flags, addr, addr_len);

    io_request_destroy(&request);

out:
    if (_bytes)
        *_bytes = request.transferred;

    return ret;
}

/** Get a socket option.
 * @param handle        Handle to socket.
 * @param level         Level to get option from.
 * @param max_len       Maximum length to return.
 * @param opt_name      Option to get.
 * @param _opt_value    Where to store option value.
 * @param _opt_len      Where to store option length.
 * @return              Status code describing result of the operation. */
status_t socket_getsockopt(
    object_handle_t *handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len)
{
    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle)
        return STATUS_INVALID_HANDLE;

    socket_t *socket = fhandle->socket;

    if (!socket->ops->getsockopt)
        return STATUS_NOT_SUPPORTED;

    return socket->ops->getsockopt(socket, level, opt_name, max_len, _opt_value, _opt_len);
}

/** Set a socket option.
 * @param handle        Handle to socket.
 * @param level         Level to set option at.
 * @param opt_name      Option to set.
 * @param opt_value     Option value.
 * @param opt_len       Option length.
 * @return              Status code describing result of the operation. */
status_t socket_setsockopt(
    object_handle_t *handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    file_handle_t *fhandle = get_socket_handle(handle);
    if (!fhandle)
        return STATUS_INVALID_HANDLE;

    socket_t *socket = fhandle->socket;

    if (!socket->ops->setsockopt)
        return STATUS_NOT_SUPPORTED;

    return socket->ops->setsockopt(socket, level, opt_name, opt_value, opt_len);
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
    sa_family_t _family, int type, int protocol, uint32_t flags,
    object_handle_t **_handle)
{
    status_t ret;

    assert(_handle);

    socket_family_t *family = socket_family_get(_family);
    if (!family)
        return STATUS_ADDR_NOT_SUPPORTED;

    socket_t *socket;
    ret = family->create(family->id, type, protocol, &socket);
    if (ret != STATUS_SUCCESS) {
        socket_family_release(family->id);
        return ret;
    }

    assert(socket->ops);

    socket->file.type = FILE_TYPE_SOCKET;
    socket->file.ops  = &socket_file_ops;
    socket->family    = family->id;

    file_handle_t *handle = file_handle_alloc(&socket->file, FILE_ACCESS_READ | FILE_ACCESS_WRITE, flags);
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

/** Bind a socket to a local address.
 * @param handle        Handle to socket to bind.
 * @param addr          Local address to bind.
 * @param addr_len      Length of local address.
 * @return              Status code describing result of the operation. */
status_t kern_socket_bind(handle_t handle, const sockaddr_t *addr, socklen_t addr_len) {
    status_t ret;

    if (!addr || addr_len == 0 || addr_len > SOCKADDR_STORAGE_SIZE)
        return STATUS_INVALID_ARG;

    void *kaddr __cleanup_kfree = kmalloc(addr_len, MM_KERNEL);

    ret = memcpy_from_user(kaddr, addr, addr_len);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    return socket_bind(khandle, kaddr, addr_len);
}

/** Initiate a connection on a socket.
 * @param handle        Handle to socket to connect.
 * @param addr          Destination address.
 * @param addr_len      Length of destination address.
 * @return              Status code describing result of the operation. */
status_t kern_socket_connect(handle_t handle, const sockaddr_t *addr, socklen_t addr_len) {
    status_t ret;

    if (!addr || addr_len == 0 || addr_len > SOCKADDR_STORAGE_SIZE)
        return STATUS_INVALID_ARG;

    void *kaddr __cleanup_kfree = kmalloc(addr_len, MM_KERNEL);

    ret = memcpy_from_user(kaddr, addr, addr_len);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    return socket_connect(khandle, kaddr, addr_len);
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

/** Receives data from a socket.
 * @param handle        Handle to socket to receive from.
 * @param buf           Buffer to receive data into.
 * @param size          Maximum size of data to receive.
 * @param flags         Behaviour flags (MSG_*).
 * @param max_addr_len  Maximum length of returned address (size of buffer).
 * @param _bytes        Where to store number of bytes received.
 * @param _addr         Where to return address of the source (can be NULL).
 * @param _addr_len     Where to return actual size of the source address. Can
 *                      be larger than max_addr_len, in which case the address
 *                      will have been truncated.
 * @return              Status code describing result of the operation. */
status_t kern_socket_recvfrom(
    handle_t handle, void *buf, size_t size, int flags, socklen_t max_addr_len,
    size_t *_bytes, sockaddr_t *_addr, socklen_t *_addr_len)
{
    status_t ret, err;

    io_request_t request;
    request.transferred = 0;

    socklen_t addr_len = 0;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    void *kaddr __cleanup_kfree = NULL;

    if (!buf ||
        (max_addr_len > 0 && (!_addr || !_addr_len)) ||
        max_addr_len > SOCKADDR_STORAGE_SIZE)
    {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    file_handle_t *fhandle = get_socket_handle(khandle);
    if (!fhandle) {
        ret = STATUS_INVALID_HANDLE;
        goto out;
    }

    io_vec_t vec;
    vec.buffer = buf;
    vec.size   = size;

    ret = io_request_init(&request, &vec, 1, 0, IO_OP_READ, IO_TARGET_USER);
    if (ret != STATUS_SUCCESS)
        goto out;

    if (max_addr_len > 0)
        kaddr = kmalloc(max_addr_len, MM_KERNEL);

    ret = socket_do_receive(fhandle, &request, flags, max_addr_len, kaddr, (kaddr) ? &addr_len : NULL);

    io_request_destroy(&request);

out:
    if (_addr_len) {
        err = write_user(_addr_len, addr_len);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    if (addr_len > 0) {
        /* Address can be truncated if family addresses are larger than
         * max_addr_len. */
        err = memcpy_to_user(_addr, kaddr, min(addr_len, max_addr_len));
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    return ret;
}

/** Sends data on a socket.
 * @param handle        Handle to socket to send on.
 * @param buf           Buffer containing data to send.
 * @param size          Size of data to send.
 * @param flags         Behaviour flags (MSG_*).
 * @param addr          Address of the destination, for connectionless-mode
 *                      sockets (can be NULL). Ignored for connection-mode
 *                      sockets.
 * @param addr_len      Size of the address structure.
 * @param _bytes        Where to store number of bytes sent.
 * @return              Status code describing result of the operation. */
status_t kern_socket_sendto(
    handle_t handle, const void *buf, size_t size, int flags,
    const sockaddr_t *addr, socklen_t addr_len, size_t *_bytes)
{
    status_t ret, err;

    io_request_t request;
    request.transferred = 0;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    void *kaddr __cleanup_kfree = NULL;

    if (!buf || (addr_len > 0 && !addr) || addr_len > SOCKADDR_STORAGE_SIZE) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    if (addr_len > 0) {
        kaddr = kmalloc(addr_len, MM_KERNEL);

        ret = memcpy_from_user(kaddr, addr, addr_len);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    file_handle_t *fhandle = get_socket_handle(khandle);
    if (!fhandle) {
        ret = STATUS_INVALID_HANDLE;
        goto out;
    }

    io_vec_t vec;
    vec.buffer = (void *)buf;
    vec.size   = size;

    ret = io_request_init(&request, &vec, 1, 0, IO_OP_WRITE, IO_TARGET_USER);
    if (ret != STATUS_SUCCESS)
        goto out;

    ret = socket_do_send(fhandle, &request, flags, kaddr, addr_len);

    io_request_destroy(&request);

out:
    if (_bytes) {
        err = write_user(_bytes, request.transferred);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    return ret;
}

/** Get a socket option.
 * @param handle        Handle to socket.
 * @param level         Level to get option from.
 * @param max_len       Maximum length to return.
 * @param opt_name      Option to get.
 * @param _opt_value    Where to store option value.
 * @param _opt_len      Where to store option length.
 * @return              Status code describing result of the operation. */
status_t kern_socket_getsockopt(
    handle_t handle, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len)
{
    status_t ret;

    if (!_opt_value || !_opt_len || max_len == 0 || max_len > SOCKOPT_LEN_MAX)
        return STATUS_INVALID_ARG;

    void *kopt_value __cleanup_kfree = kmalloc(max_len, MM_KERNEL);

    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    socklen_t kopt_len = 0;
    ret = socket_getsockopt(khandle, level, opt_name, max_len, kopt_value, &kopt_len);
    if (ret == STATUS_SUCCESS) {
        ret = write_user(_opt_len, kopt_len);
        if (ret == STATUS_SUCCESS)
            ret = memcpy_to_user(_opt_value, kopt_value, kopt_len);
    }

    return ret;
}

/** Set a socket option.
 * @param handle        Handle to socket.
 * @param level         Level to set option at.
 * @param opt_name      Option to set.
 * @param opt_value     Option value.
 * @param opt_len       Option length.
 * @return              Status code describing result of the operation. */
status_t kern_socket_setsockopt(
    handle_t handle, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    status_t ret;

    if (!opt_value || opt_len == 0 || opt_len > SOCKOPT_LEN_MAX)
        return STATUS_INVALID_ARG;

    void *kopt_value __cleanup_kfree = kmalloc(opt_len, MM_KERNEL);

    ret = memcpy_from_user(kopt_value, opt_value, opt_len);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *khandle __cleanup_object_handle = NULL;
    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    return socket_setsockopt(khandle, level, opt_name, kopt_value, opt_len);
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
            object_handle_detach(uhandles[0], &_handles[0]);
    }

    object_handle_release(handles[0]);
    object_handle_release(handles[1]);

    return ret;
}
