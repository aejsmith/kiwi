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

#include <kernel/device/ipv4_control.h>

#include <net/arp.h>
#include <net/ip.h>
#include <net/ipv4.h>
#include <net/packet.h>
#include <net/route.h>
#include <net/socket.h>
#include <net/tcp.h>
#include <net/udp.h>

#include <sync/rwlock.h>

#include <kernel.h>
#include <status.h>

/** Define to enable debug output. */
#define DEBUG_IPV4

#ifdef DEBUG_IPV4
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** /virtual/net/control/ipv4 */
static device_t *ipv4_control_device;

/** Next IPv4 packet ID. */
static atomic_uint16_t next_ipv4_id = 0;

/** IPv4 routing table. */
static ARRAY_DEFINE(ipv4_route_table);
static RWLOCK_DEFINE(ipv4_route_lock);

static bool is_netmask_valid(in_addr_t netmask) {
    netmask = net32_to_cpu(netmask);
    uint32_t i;
    for (i = 0; i < 32 && !(netmask & 1); i++)
        netmask >>= 1;
    for (; i < 32 && (netmask & 1); i++)
        netmask >>= 1;
    return i == 32;
}

static uint32_t netmask_width(in_addr_t netmask) {
    if (netmask == 0)
        return 0;
    return 32 - __builtin_ctz(net32_to_cpu(netmask));
}

static bool is_route_equal(const ipv4_route_t *a, const ipv4_route_t *b) {
    /* Flags are not considered for equality. */
    return
        a->addr.val     == b->addr.val &&
        a->netmask.val  == b->netmask.val &&
        a->gateway.val  == b->gateway.val &&
        a->source.val   == b->source.val &&
        a->interface_id == b->interface_id;
}

static status_t ipv4_add_route(const ipv4_route_t *route, bool is_auto) {
    status_t ret;

    /* Can't externally add an AUTO route. */
    if (!is_auto && route->flags & IPV4_ROUTE_AUTO)
        return STATUS_INVALID_ARG;

    /* Check netmask validity. */
    if (!is_netmask_valid(route->netmask.val))
        return STATUS_INVALID_ARG;

    /* Host bits in the address must be 0. */
    if (route->addr.val & ~route->netmask.val)
        return STATUS_INVALID_ARG;

    /* Source address must be on the network. */
    if ((route->source.val & route->netmask.val) != route->addr.val)
        return STATUS_INVALID_ARG;

    /* A default route must have a gateway and a zero netmask. */
    if (route->addr.val == INADDR_ANY && (route->gateway.val == INADDR_ANY || route->netmask.val != 0))
        return STATUS_INVALID_ARG;

    /* This is already locked for write when adding an automatic route. */
    if (!is_auto)
        net_interface_read_lock();

    /* Check that the interface exists. */
    net_interface_t *interface = net_interface_get(route->interface_id);
    if (!interface) {
        ret = STATUS_NET_DOWN;
        goto out_unlock_interface;
    }

    rwlock_write_lock(&ipv4_route_lock);

    /* Keep the routing table sorted from most-specific to least-specific. */
    size_t pos;
    for (pos = 0; pos < ipv4_route_table.count; pos++) {
        ipv4_route_t *entry = array_entry(&ipv4_route_table, ipv4_route_t, pos);
        if (netmask_width(route->netmask.val) > netmask_width(entry->netmask.val))
            break;

        /* Check if the entries are equal. Note that since we keep the list
         * sorted, any entries after we break out on the previous test cannot
         * be equal. */
        if (is_route_equal(route, entry)) {
            ret = STATUS_ALREADY_EXISTS;
            goto out_unlock_route;
        }
    }

    /* Add the entry. */
    ipv4_route_t *entry = array_insert(&ipv4_route_table, ipv4_route_t, pos);
    memcpy(entry, route, sizeof(*entry));

    dprintf(
        "ipv4: added route %pI4 netmask %pI4 gateway %pI4 source %pI4 on %pD\n",
        &entry->addr, &entry->netmask, &entry->gateway, &entry->source,
        net_device_from_interface(interface)->node);

    ret = STATUS_SUCCESS;

out_unlock_route:
    rwlock_unlock(&ipv4_route_lock);

out_unlock_interface:
    if (!is_auto)
        net_interface_unlock();

    return ret;
}

static status_t ipv4_remove_route(const ipv4_route_t *route, bool auto_only) {
    rwlock_write_lock(&ipv4_route_lock);

    status_t ret = STATUS_NOT_FOUND;

    for (size_t i = 0; i < ipv4_route_table.count; i++) {
        ipv4_route_t *entry = array_entry(&ipv4_route_table, ipv4_route_t, i);

        if (is_route_equal(route, entry)) {
            if (!auto_only || route->flags & IPV4_ROUTE_AUTO)
                array_remove(&ipv4_route_table, ipv4_route_t, i);

            ret = STATUS_SUCCESS;
            break;
        }
    }

    rwlock_unlock(&ipv4_route_lock);
    return ret;
}

static bool ipv4_interface_addr_valid(const net_interface_addr_t *addr) {
    const uint8_t *addr_bytes = addr->ipv4.addr.bytes;

    /* 0.0.0.0/8 is invalid as a host address. */
    if (addr_bytes[0] == 0)
        return false;

    /* 255.255.255.255 = broadcast address. */
    if (addr_bytes[0] == 255 && addr_bytes[1] == 255 && addr_bytes[2] == 255 && addr_bytes[3] == 255)
        return false;

    /* Validate netmask bits. */
    if (!is_netmask_valid(addr->ipv4.netmask.val))
        return false;

    // TODO: Anything more needed here?
    return true;
}

static bool ipv4_interface_addr_equal(const net_interface_addr_t *a, const net_interface_addr_t *b) {
    /* For equality testing interface addresses we only look at the address
     * itself, not netmask/broadcast. */
    return a->ipv4.addr.val == b->ipv4.addr.val;
}

static void ipv4_interface_remove(net_interface_t *interface) {
    /* Remove ARP cache entries corresponding to this interface. */
    arp_interface_remove(interface);

    /* Remove any routing table entries for this interface. */
    rwlock_write_lock(&ipv4_route_lock);

    for (size_t i = 0; i < ipv4_route_table.count; ) {
        ipv4_route_t *entry = array_entry(&ipv4_route_table, ipv4_route_t, i);

        if (entry->interface_id == interface->id) {
            array_remove(&ipv4_route_table, ipv4_route_t, i);
        } else {
            i++;
        }
    }

    rwlock_unlock(&ipv4_route_lock);
}

static void route_from_interface_addr(
    net_interface_t *interface, const net_interface_addr_t *addr,
    ipv4_route_t *route)
{
    route->addr.val     = addr->ipv4.addr.val & addr->ipv4.netmask.val;
    route->netmask.val  = addr->ipv4.netmask.val;
    route->gateway.val  = INADDR_ANY;
    route->source.val   = addr->ipv4.addr.val;
    route->interface_id = interface->id;
    route->flags        = IPV4_ROUTE_AUTO;
}

static void ipv4_interface_add_addr(net_interface_t *interface, const net_interface_addr_t *addr) {
    /* Add a routing table entry for this address. */
    ipv4_route_t route;
    route_from_interface_addr(interface, addr, &route);

    status_t ret = ipv4_add_route(&route, true);
    if (ret != STATUS_SUCCESS) {
        kprintf(
            LOG_ERROR, "ipv4: failed to add route for interface address %pI4: %" PRId32,
            &addr->ipv4.addr, ret);
    }
}

static void ipv4_interface_remove_addr(net_interface_t *interface, const net_interface_addr_t *addr) {
    /* Remove any automatically added entry for this address. It could have
     * been manually removed so don't worry about failure. */
    ipv4_route_t route;
    route_from_interface_addr(interface, addr, &route);
    ipv4_remove_route(&route, true);
}

static status_t ipv4_socket_route(net_socket_t *socket, const sockaddr_t *_dest_addr, net_route_t *route) {
    const sockaddr_in_t *dest_addr = (const sockaddr_in_t *)_dest_addr;

    route->source_addr.family  = AF_INET;
    route->dest_addr.family    = AF_INET;
    route->gateway_addr.family = AF_INET;

    rwlock_read_lock(&ipv4_route_lock);

    /* This is sorted most-specific to least specific, so pick the first match. */
    status_t ret = STATUS_NET_UNREACHABLE;
    for (size_t i = 0; i < ipv4_route_table.count; i++) {
        const ipv4_route_t *entry = array_entry(&ipv4_route_table, ipv4_route_t, i);

        if ((dest_addr->sin_addr.val & entry->netmask.val) == entry->addr.val) {
            route->interface_id          = entry->interface_id;
            route->source_addr.ipv4.val  = entry->source.val;
            route->dest_addr.ipv4.val    = dest_addr->sin_addr.val;

            /* If this route has a gateway use that, else use the destination. */
            route->gateway_addr.ipv4.val = (entry->gateway.val != INADDR_ANY)
                ? entry->gateway.val
                : dest_addr->sin_addr.val;

            dprintf(
                "ipv4: routing %pI4 to %pI4 netmask %pI4 gateway %pI4 source %pI4\n",
                &dest_addr->sin_addr, &entry->addr, &entry->netmask, &route->gateway_addr.ipv4, &entry->source);

            ret = STATUS_SUCCESS;
            break;
        }
    }

    rwlock_unlock(&ipv4_route_lock);
    return ret;
}

static status_t ipv4_socket_transmit(net_socket_t *socket, net_packet_t *packet, const net_route_t *route) {
    status_t ret;

    if (packet->size > IPV4_MTU)
        return STATUS_MSG_TOO_LONG;

    const net_addr_ipv4_t *source_addr  = &route->source_addr.ipv4;
    const net_addr_ipv4_t *dest_addr    = &route->dest_addr.ipv4;
    const net_addr_ipv4_t *gateway_addr = &route->gateway_addr.ipv4;

    /* Find our destination hardware address. */
    uint8_t dest_hw_addr[NET_DEVICE_ADDR_MAX];
    ret = arp_lookup(route->interface_id, source_addr, gateway_addr, dest_hw_addr);
    if (ret != STATUS_SUCCESS)
        return ret;

    net_interface_read_lock();

    net_interface_t *interface = net_interface_get(route->interface_id);
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
    header->source_addr       = source_addr->val;
    header->dest_addr         = dest_addr->val;

    /* Calculate checksum based on header with checksum initialised to 0. */
    header->checksum = ip_checksum(header, sizeof(*header));

    packet->type = NET_PACKET_TYPE_IPV4;

    ret = net_interface_transmit(interface, packet, dest_hw_addr);

out_unlock:
    net_interface_unlock();
    return ret;
}

const net_family_t ipv4_net_family = {
    .mtu                    = IPV4_MTU,
    .interface_addr_len     = sizeof(net_interface_addr_ipv4_t),
    .socket_addr_len        = sizeof(sockaddr_in_t),

    .interface_addr_valid   = ipv4_interface_addr_valid,
    .interface_addr_equal   = ipv4_interface_addr_equal,

    .interface_remove       = ipv4_interface_remove,
    .interface_add_addr     = ipv4_interface_add_addr,
    .interface_remove_addr  = ipv4_interface_remove_addr,

    .socket_route           = ipv4_socket_route,
    .socket_transmit        = ipv4_socket_transmit,
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
    socket->family = &ipv4_net_family;
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
        const net_interface_addr_t *interface_addr = array_entry(&interface->addrs, net_interface_addr_t, i);

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
        net_addr_t source_addr;
        source_addr.family   = AF_INET;
        source_addr.ipv4.val = header->source_addr;

        net_addr_t dest_addr;
        dest_addr.family     = AF_INET;
        dest_addr.ipv4.val   = header->dest_addr;

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

static status_t ipv4_control_device_request(
    device_t *device, file_handle_t *handle, unsigned request,
    const void *in, size_t in_size, void **_out, size_t *_out_size)
{
    status_t ret;

    switch (request) {
        case IPV4_CONTROL_DEVICE_REQUEST_ADD_ROUTE:
            if (in_size != sizeof(ipv4_route_t)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            ret = ipv4_add_route((const ipv4_route_t *)in, false);
            break;

        case IPV4_CONTROL_DEVICE_REQUEST_REMOVE_ROUTE:
            if (in_size != sizeof(ipv4_route_t)) {
                ret = STATUS_INVALID_ARG;
                break;
            }

            ret = ipv4_remove_route((const ipv4_route_t *)in, false);
            break;

        // TODO: If we add a route query, make sure to zero the returned
        // structure properly.

        default:
            ret = STATUS_INVALID_REQUEST;
            break;
    }

    return ret;
}

static const device_ops_t ipv4_control_device_ops = {
    .type    = FILE_TYPE_CHAR,
    .request = ipv4_control_device_request,
};

void ipv4_init(void) {
    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = IPV4_CONTROL_DEVICE_CLASS_NAME } },
    };

    status_t ret = device_create(
        "ipv4", net_control_device, &ipv4_control_device_ops, NULL, attrs,
        array_size(attrs), &ipv4_control_device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create /virtual/net/control/ipv4: %" PRId32, ret);

    device_publish(ipv4_control_device);
}
