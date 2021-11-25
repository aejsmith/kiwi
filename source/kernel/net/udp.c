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
 * @brief               UDP protocol implementation.
 */

#include <mm/malloc.h>

#include <io/request.h>

#include <net/interface.h>
#include <net/ip.h>
#include <net/packet.h>
#include <net/udp.h>

#include <assert.h>

static void udp_socket_close(socket_t *_socket) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    kfree(socket);
}

static uint16_t udp_checksum(
    void *data, size_t size, const sockaddr_ip_t *source_addr,
    const sockaddr_ip_t *dest_addr)
{
    // TODO: optional in IPv4 so can do without for now...
    // if 0 return 0xffff
    return 0;
}

static status_t udp_socket_send(
    socket_t *_socket, io_request_t *request, int flags, const sockaddr_t *addr,
    socklen_t addr_len)
{
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));
    status_t ret;

    const sockaddr_ip_t *dest_addr;
    if (addr_len == 0) {
        // TODO: connect() should be able to set a default address if this is NULL.
        return STATUS_DEST_ADDR_REQUIRED;
    } else {
        ret = net_socket_addr_valid(&socket->net, addr, addr_len);
        if (ret != STATUS_SUCCESS)
            return ret;

        dest_addr = (const sockaddr_ip_t *)addr;
    }

    /* Check packet size. */
    size_t packet_size = sizeof(udp_header_t) + request->total;
    if (packet_size > UDP_MAX_PACKET_SIZE || packet_size > socket->net.family_ops->mtu)
        return STATUS_MSG_TOO_LONG;

    udp_header_t *header;
    net_packet_t *packet = net_packet_kmalloc(packet_size, MM_USER, (void **)&header);
    if (!packet)
        return STATUS_NO_MEMORY;

    void *data = &header[1];
    ret = io_request_copy(request, data, request->total, false);
    if (ret != STATUS_SUCCESS)
        goto out_release;

    net_addr_read_lock();

    /* Calculate a route for the packet. */
// TODO: For sockets bound to a specific address use that source.
    net_interface_t *interface;
    sockaddr_ip_t source_addr;
    ret = net_socket_route(&socket->net, &dest_addr->addr, &interface, &source_addr.addr);
    if (ret != STATUS_SUCCESS)
        goto out_unlock;

    /* Initialise header. */
    header->length      = cpu_to_net16(packet_size);
    header->dest_port   = net_socket_addr_port(&socket->net, &dest_addr->addr);
// TODO! ephemeral port assignment (or bound address)
    header->source_port = cpu_to_net16(49152);
    header->checksum    = 0;

    /* Calculate checksum based on header with checksum initialised to 0. */
    header->checksum = udp_checksum(header, packet_size, &source_addr, dest_addr);

    ret = net_socket_transmit(&socket->net, packet, interface, &source_addr.addr, &dest_addr->addr);
    if (ret == STATUS_SUCCESS)
        request->transferred += request->total;

out_unlock:
    net_addr_unlock();

out_release:
    net_packet_release(packet);
    return ret;
}

static status_t udp_socket_receive(
    socket_t *_socket, io_request_t *request, int flags, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

// locking needed...?
    (void)socket;
    return STATUS_NOT_IMPLEMENTED;
}

static const socket_ops_t udp_socket_ops = {
    .close   = udp_socket_close,
    .send    = udp_socket_send,
    .receive = udp_socket_receive,
};

/** Creates a UDP socket. */
status_t udp_socket_create(sa_family_t family, socket_t **_socket) {
    assert(family == AF_INET || family == AF_INET6);

    udp_socket_t *socket = kmalloc(sizeof(udp_socket_t), MM_KERNEL);

    socket->net.socket.ops = &udp_socket_ops;
    socket->net.protocol   = IPPROTO_UDP;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}
