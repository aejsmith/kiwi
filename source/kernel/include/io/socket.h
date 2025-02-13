/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

struct io_request;
struct socket;

/** Socket operations structure. */
typedef struct socket_ops {
    /** Closes and frees the socket.
     * @param socket        Socket to close. */
    void (*close)(struct socket *socket);

    /** Gets the name of the socket.
     * @param socket        Socket to get name of.
     * @return              Pointer to allocated name string. */
    char *(*name)(struct socket *socket);

    /** Get the name of a file in KDB context.
     * @see                 object_type_t::name().
     * @param socket        Socket to get name of.
     * @param buf           Buffer to write into.
     * @param size          Size of the buffer.
     * @return              Pointer to start of name string, or NULL if not
     *                      available. */
    char *(*name_unsafe)(struct socket *socket, char *buf, size_t size);

    /** Signals that a socket event is being waited for.
     * @see                 file_ops_t::wait
     * @param socket        Socket to wait on.
     * @param event         Event that is being waited for.
     * @return              Status code describing result of the operation. */
    status_t (*wait)(struct socket *socket, object_event_t *event);

    /** Stops waiting for a socket event.
     * @param socket        Socket to stop waiting on.
     * @param event         Event that is being waited for. */
    void (*unwait)(struct socket *socket, object_event_t *event);

    /** Bind a socket to a local address.
     * @param socket        Socket to bind.
     * @param addr          Local address to bind.
     * @param addr_len      Length of local address.
     * @return              Status code describing result of the operation. */
    status_t (*bind)(struct socket *socket, const sockaddr_t *addr, socklen_t addr_len);

    /** Initiate a connection on a socket.
     * @param socket        Socket to connect.
     * @param addr          Destination address.
     * @param addr_len      Length of destination address.
     * @return              Status code describing result of the operation. */
    status_t (*connect)(struct socket *socket, const sockaddr_t *addr, socklen_t addr_len);

    /** Get the address of the peer that a socket is connected to.
     * @param handle        Socket to get peer address from.
     * @param max_len       Maximum length of returned address (size of buffer).
     * @param _addr         Where to return address of the peer (can be NULL).
     * @param _addr_len     Where to return actual size of the peer address.
     * @return              Status code describing result of the operation. */
    status_t (*getpeername)(
        struct socket *socket, socklen_t max_len, sockaddr_t *_addr,
        socklen_t *_addr_len);

    /** Get the address that a socket is bound to.
     * @param handle        Socket to get address from.
     * @param max_len       Maximum length of returned address (size of buffer).
     * @param _addr         Where to return address of the socket (can be NULL).
     * @param _addr_len     Where to return actual size of the socket address.
     * @return              Status code describing result of the operation. */
    status_t (*getsockname)(
        struct socket *socket, socklen_t max_len, sockaddr_t *_addr,
        socklen_t *_addr_len);

    /** Sends data on the socket.
     * @param socket        Socket to send on.
     * @param request       I/O request containing data.
     * @param flags         Behaviour flags (MSG_*).
     * @param addr          Destination address.
     * @param addr_len      Length of destination address (0 for unspecified).
     * @return              Status code describing result of the operation. */
    status_t (*send)(
        struct socket *socket, struct io_request *request, int flags,
        const sockaddr_t *addr, socklen_t addr_len);

    /** Receives data from the socket.
     * @param socket        Socket to receive from.
     * @param request       I/O request to receive data.
     * @param flags         Behaviour flags (MSG_*).
     * @param max_addr_len  Maximum length of returned address (size of buffer,
     *                      can be 0 if no address to be returned).
     * @param _addr         Where to return address of the source.
     * @param _addr_len     Where to return actual size of the source address.
     * @return              Status code describing result of the operation. */
    status_t (*receive)(
        struct socket *socket, struct io_request *request, int flags,
        socklen_t max_addr_len, sockaddr_t *_addr, socklen_t *_addr_len);

    /** Get a socket option.
     * @param socket        Socket to get option from.
     * @param level         Level to get option from.
     * @param max_len       Maximum length to return.
     * @param opt_name      Option to get.
     * @param _opt_value    Where to store option value.
     * @param _opt_len      Where to store option length.
     * @return              Status code describing result of the operation. */
    status_t (*getsockopt)(
        struct socket *socket, int level, int opt_name, socklen_t max_len,
        void *_opt_value, socklen_t *_opt_len);

    /** Set a socket option.
     * @param socket        Socket to set option on.
     * @param level         Level to set option at.
     * @param opt_name      Option to set.
     * @param opt_value     Option value.
     * @param opt_len       Option length.
     * @return              Status code describing result of the operation. */
    status_t (*setsockopt)(
        struct socket *socket, int level, int opt_name, const void *opt_value,
        socklen_t opt_len);
} socket_ops_t;

/** Base socket structure (embedded in protocol-specific implementation). */
typedef struct socket {
    file_t file;                    /**< File header. */
    sa_family_t family;             /**< Address family ID (AF_*). */
    const socket_ops_t *ops;        /**< Operations implementing the socket. */
} socket_t;

/** Structure describing a supported socket family. */
typedef struct socket_family {
    list_t link;                    /**< Link to families list. */
    uint32_t count;                 /**< Number of sockets open using the family. */
    sa_family_t id;                 /**< Family ID (AF_*). */

    /** Creates a socket.
     * @param family        Address family of the socket.
     * @param type          Type of the socket.
     * @param protocol      Protocol number.
     * @param _socket       Where to store pointer to created socket.
     * @return              Status code describing the result of the operation. */
    status_t (*create)(sa_family_t family, int type, int protocol, socket_t **_socket);
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
