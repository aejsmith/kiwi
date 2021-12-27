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

#include <device/net/net.h>

#include <net/arp.h>
#include <net/ip.h>
#include <net/ipv4.h>
#include <net/packet.h>
#include <net/socket.h>
#include <net/tcp.h>
#include <net/udp.h>

#include <kernel.h>
#include <status.h>

/** Define to enable debug output. */
#define DEBUG_IPV4

#ifdef DEBUG_IPV4
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Next IPv4 packet ID. */
static atomic_uint16_t next_ipv4_id = 0;

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

static status_t ipv4_route(
    net_socket_t *socket, const sockaddr_t *_dest_addr, uint32_t *_interface_id,
    sockaddr_t *_source_addr)
{
    // TODO: Proper configurable routing table. That routing table should be
    // based on interface indices with its own separate lock, so we don't need
    // to deal with the interface list here at all.

    const sockaddr_in_t *dest_addr = (const sockaddr_in_t *)_dest_addr;
    sockaddr_in_t *source_addr     = (sockaddr_in_t *)_source_addr;

    source_addr->sin_family = AF_INET;
    source_addr->sin_port   = 0;

    net_interface_read_lock();

    /* Find a suitable route based on interface addresses. */
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

                    *_interface_id = interface->id;
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

    net_interface_unlock();
    return ret;
}

static status_t ipv4_transmit(
    net_socket_t *socket, net_packet_t *packet, uint32_t interface_id,
    const sockaddr_t *_source_addr, const sockaddr_t *_dest_addr)
{
    status_t ret;

    if (packet->size > IPV4_MTU)
        return STATUS_MSG_TOO_LONG;

    const sockaddr_in_t *dest_addr   = (const sockaddr_in_t *)_dest_addr;
    const sockaddr_in_t *source_addr = (const sockaddr_in_t *)_source_addr;

    /* Find our destination hardware address. */
    // TODO: Use gateway IP for default route.
    uint8_t dest_hw_addr[NET_DEVICE_ADDR_MAX];
    ret = arp_lookup(interface_id, &source_addr->sin_addr, &dest_addr->sin_addr, dest_hw_addr);
    if (ret != STATUS_SUCCESS)
        return ret;

    net_interface_read_lock();

    net_interface_t *interface = net_interface_get(interface_id);
    if (!interface) {
        ret = STATUS_NET_DOWN;
        goto out_unlock;
    }

    net_device_t *device = net_device_from_interface(interface);

    // TODO: Fragmentation.
    if (packet->size + sizeof(ipv4_header_t) > device->mtu) {
        ret = STATUS_MSG_TOO_LONG;
        goto out_unlock;
    }

    ipv4_header_t *header;
    net_buffer_t *buffer = net_buffer_kmalloc(sizeof(*header), MM_KERNEL, (void **)&header);
    net_packet_prepend(packet, buffer);

    uint16_t id = atomic_fetch_add_explicit(&next_ipv4_id, 1, memory_order_relaxed);

    header->version           = 4;
    header->ihl               = sizeof(*header) / 4;
    header->dscp_ecn          = 0;
    header->total_size        = cpu_to_net16(packet->size);
    header->id                = cpu_to_net16(id);
    header->frag_offset_flags = 0;
    header->ttl               = 64;
    header->protocol          = socket->protocol;
    header->checksum          = 0;
    header->source_addr       = source_addr->sin_addr.val;
    header->dest_addr         = dest_addr->sin_addr.val;

    /* Calculate checksum based on header with checksum initialised to 0. */
    header->checksum = ip_checksum(header, sizeof(*header));

    packet->type = NET_PACKET_TYPE_IPV4;

    ret = net_interface_transmit(interface, packet, dest_hw_addr);

out_unlock:
    net_interface_unlock();
    return ret;
}

static const net_family_ops_t ipv4_net_family_ops = {
    .mtu      = IPV4_MTU,
    .addr_len = sizeof(sockaddr_in_t),

    .route    = ipv4_route,
    .transmit = ipv4_transmit,
};

/** Creates an IPv4 socket. */
status_t ipv4_socket_create(sa_family_t family, int type, int protocol, socket_t **_socket) {
    status_t ret = STATUS_PROTO_NOT_SUPPORTED;

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
        case SOCK_STREAM: {
            switch (protocol) {
                case IPPROTO_IP:
                case IPPROTO_TCP:
                    ret = tcp_socket_create(family, _socket);
                    break;
            }

            break;
        }
    }

    if (ret != STATUS_SUCCESS)
        return ret;

    net_socket_t *socket = cast_net_socket(*_socket);
    socket->family_ops = &ipv4_net_family_ops;
    return STATUS_SUCCESS;
}

/** Handles a received IPv4 packet.
 * @param interface     Source interface.
 * @param packet        Packet that was received. */
void ipv4_receive(net_interface_t *interface, net_packet_t *packet) {
    /* Get and validate the header. */
    const ipv4_header_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (!header) {
        dprintf("ipv4: dropping packet: too short for header\n");
        return;
    } else if (header->version != 4) {
        dprintf("ipv4: dropping packet: incorrect version (%" PRIu8 ")\n", header->version);
        return;
    } else if (header->ihl < sizeof(*header) / 4) {
        dprintf("ipv4: dropping packet: IHL too short (%" PRIu8 ")\n", header->ihl);
        return;
    }

    // TODO: Fragmentation.
    if (net16_to_cpu(header->frag_offset_flags) & (IPV4_HEADER_FRAG_OFFSET_MASK | IPV4_HEADER_FRAG_FLAGS_MF)) {
        dprintf("ipv4: dropping packet: fragmentation unsupported\n");
        return;
    }

    uint16_t total_size  = net16_to_cpu(header->total_size);
    uint16_t header_size = header->ihl * 4;
    if (total_size > packet->size) {
        dprintf(
            "ipv4: dropping packet: packet size mismatch (total_size: %" PRIu16 ", packet_size: %" PRIu16 ")\n",
            total_size, packet->size);
        return;
    } else if (header_size > total_size) {
        dprintf(
            "ipv4: dropping packet: header size exceeds packet size (header_size: %" PRIu16 ", total_size: %" PRIu16 ")\n",
            header_size, total_size);
        return;
    }

    /* Checksum the header. */
    if (ip_checksum(header, sizeof(*header)) != 0) {
        dprintf("ipv4: dropping packet: checksum failed\n");
        return;
    }

    /* Check whether this packet is destined for us. */
    bool found_addr = false;
    for (size_t i = 0; i < interface->addrs.count && !found_addr; i++) {
        const net_addr_t *interface_addr = array_entry(&interface->addrs, net_addr_t, i);

        if (interface_addr->family == AF_INET) {
            found_addr =
                header->dest_addr == interface_addr->ipv4.addr.val ||
                header->dest_addr == interface_addr->ipv4.broadcast.val;
        }
    }
    if (!found_addr) {
        dprintf("ipv4: dropping packet: not destined for us\n");
        return;
    }

    dprintf(
        "ipv4: received %" PRIu16 " byte packet with protocol %" PRIu8 "\n",
        total_size, header->protocol);

    /* Remove header and subset to the actual data size specified by the header
     * for the protocol. */
    uint16_t data_size = total_size - header_size;
    if (data_size > 0) {
        sockaddr_ip_t source_addr;
        memset(&source_addr, 0, sizeof(source_addr));
        source_addr.ipv4.sin_family   = AF_INET;
        source_addr.ipv4.sin_addr.val = header->source_addr;

        sockaddr_ip_t dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.ipv4.sin_family     = AF_INET;
        dest_addr.ipv4.sin_addr.val   = header->dest_addr;

        net_packet_subset(packet, header_size, data_size);

        // TODO: Would be good to release net_interface_lock past here.
        switch (header->protocol) {
            case IPPROTO_TCP:
                tcp_receive(packet, &source_addr, &dest_addr);
                break;
            case IPPROTO_UDP:
                udp_receive(packet, &source_addr, &dest_addr);
                break;
        }
    }
}
