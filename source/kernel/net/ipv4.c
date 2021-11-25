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
 * @brief               Internet Protocol v4 implementation.
 */

#include <net/ipv4.h>
#include <net/packet.h>
#include <net/socket.h>
#include <net/udp.h>

#include <kernel.h>
#include <status.h>

static bool ipv4_net_addr_valid(const net_addr_t *addr) {
    const uint8_t *addr_bytes = addr->ipv4.addr.bytes;

    /* 0.0.0.0/8 is invalid as a host address. */
    if (addr_bytes[0] == 0)
        return false;

    /* 255.255.255.255 = broadcast address. */
    if (addr_bytes[0] == 255 && addr_bytes[1] == 255 && addr_bytes[2] == 255 && addr_bytes[3] == 255)
        return false;

    // TODO: Anything more needed here? Netmask validation?
    return true;
}

static bool ipv4_net_addr_equal(const net_addr_t *a, const net_addr_t *b) {
    /* For equality testing interface addresses we only look at the address
     * itself, not netmask/broadcast. */
    return a->ipv4.addr.val == b->ipv4.addr.val;
}

const net_addr_ops_t ipv4_net_addr_ops = {
    .len   = sizeof(net_addr_ipv4_t),
    .valid = ipv4_net_addr_valid,
    .equal = ipv4_net_addr_equal,
};

static uint16_t ipv4_addr_port(const net_socket_t *socket, const sockaddr_t *_addr) {
    const sockaddr_in_t *addr = (const sockaddr_in_t *)_addr;

    return addr->sin_port;
}

static status_t ipv4_route(
    net_socket_t *socket, const sockaddr_t *_dest_addr,
    net_interface_t **_interface, sockaddr_t *_source_addr)
{
    // TODO: Proper configurable routing table.

    const sockaddr_in_t *dest_addr = (const sockaddr_in_t *)_dest_addr;
    sockaddr_in_t *source_addr     = (sockaddr_in_t *)_source_addr;

    source_addr->sin_family = AF_INET;
    source_addr->sin_port   = 0;

    /* Find a suitable route based on interface addresses. net_addr_lock should
     * be held. */
    status_t ret = STATUS_NET_UNREACHABLE;
    list_foreach(&net_interface_list, iter) {
        net_interface_t *interface = list_entry(iter, net_interface_t, interfaces_link);

        for (size_t i = 0; i < interface->addrs.count; i++) {
            const net_addr_t *interface_addr = array_entry(&interface->addrs, net_addr_t, i);

            if (interface_addr->family == AF_INET) {
                const in_addr_t dest_net      = dest_addr->sin_addr.val & interface_addr->ipv4.netmask.val;
                const in_addr_t interface_net = interface_addr->ipv4.addr.val & interface_addr->ipv4.netmask.val;

                if (dest_net == interface_net) {
                    source_addr->sin_addr.val = interface_addr->ipv4.addr.val;
                    ret = STATUS_SUCCESS;
                    break;
                }
            }
        }

        if (ret == STATUS_SUCCESS)
            break;
    }

    if (ret == STATUS_NET_UNREACHABLE) {
        // TODO: Default route.
    }

    return ret;
}

static status_t ipv4_transmit(
    net_socket_t *socket, net_packet_t *packet, net_interface_t *interface,
    const sockaddr_t *source_addr, const sockaddr_t *dest_addr)
{
    return STATUS_NOT_IMPLEMENTED;
}

static const net_family_ops_t ipv4_net_family_ops = {
    .mtu       = IPV4_MTU,
    .addr_len  = sizeof(sockaddr_in_t),

    .addr_port = ipv4_addr_port,
    .route     = ipv4_route,
    .transmit  = ipv4_transmit,
};

/** Creates an IPv4 socket. */
status_t ipv4_socket_create(sa_family_t family, int type, int protocol, socket_t **_socket) {
    status_t ret = STATUS_INVALID_ARG;

    switch (type) {
        case SOCK_DGRAM: {
            switch (protocol) {
                case IPPROTO_IP:
                case IPPROTO_UDP:
                    ret = udp_socket_create(family, _socket);
                    break;
            }

            break;
        }
        //case SOCK_STREAM: {
        //  break;
        //}
    }

    if (ret != STATUS_SUCCESS)
        return ret;

    net_socket_t *socket = cast_net_socket(*_socket);

    socket->family_ops = &ipv4_net_family_ops;

    return STATUS_SUCCESS;
}
