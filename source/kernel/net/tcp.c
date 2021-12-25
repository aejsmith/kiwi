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
 * @brief               TCP protocol implementation.
 */

#include <io/request.h>

#include <mm/malloc.h>

#include <net/packet.h>
#include <net/tcp.h>

#include <assert.h>
#include <status.h>

/** Define to enable debug output. */
#define DEBUG_TCP

#ifdef DEBUG_TCP
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** TCP socket structure. */
typedef struct tcp_socket {
    net_socket_t net;
} tcp_socket_t;

DEFINE_CLASS_CAST(tcp_socket, net_socket, net);

static void tcp_socket_close(socket_t *_socket) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    kfree(socket);
}

static status_t tcp_socket_connect(socket_t *socket, const sockaddr_t *addr, socklen_t addr_len) {
    return STATUS_NOT_IMPLEMENTED;
}

static status_t tcp_socket_send(
    socket_t *_socket, io_request_t *request, int flags, const sockaddr_t *addr,
    socklen_t addr_len)
{
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    (void)socket;
    return STATUS_NOT_IMPLEMENTED;
}

static status_t tcp_socket_receive(
    socket_t *_socket, io_request_t *request, int flags, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    (void)socket;
    return STATUS_NOT_IMPLEMENTED;
}

static const socket_ops_t tcp_socket_ops = {
    .close   = tcp_socket_close,
    .connect = tcp_socket_connect,
    .send    = tcp_socket_send,
    .receive = tcp_socket_receive,
};

/** Creates a TCP socket. */
status_t tcp_socket_create(sa_family_t family, socket_t **_socket) {
    assert(family == AF_INET || family == AF_INET6);

    tcp_socket_t *socket = kmalloc(sizeof(tcp_socket_t), MM_KERNEL);

    socket->net.socket.ops = &tcp_socket_ops;
    socket->net.protocol   = IPPROTO_TCP;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}

/** Handles a received TCP packet. */
void tcp_receive(net_packet_t *packet, const sockaddr_ip_t *source_addr, const sockaddr_ip_t *dest_addr) {
    dprintf("tcp: packet received\n");
}
