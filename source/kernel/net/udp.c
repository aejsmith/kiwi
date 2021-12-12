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
#include <mm/page.h>

#include <io/request.h>

#include <net/interface.h>
#include <net/ip.h>
#include <net/packet.h>
#include <net/udp.h>

#include <sync/condvar.h>

#include <assert.h>

/** Define to enable debug output. */
#define DEBUG_UDP

#ifdef DEBUG_UDP
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Maximum receive queue size. */
// TODO: Make this configurable.
#define UDP_RX_QUEUE_MAX_SIZE       PAGE_SIZE

/** Range to use for ephemeral (dynamic) ports (IANA standard range). */
#define UDP_EPHEMERAL_PORT_FIRST    49152
#define UDP_EPHEMERAL_PORT_LAST     65535

/** Received UDP packet structure. */
typedef struct udp_rx_packet {
    list_t link;                    /**< Link to RX queue. */
    sockaddr_ip_t source_addr;      /**< Source address. */
    uint16_t size;                  /**< Data size. */
    uint8_t data[];                 /**< Packet payload. */
} udp_rx_packet_t;

/** UDP socket structure. */
typedef struct udp_socket {
    net_socket_t net;

    list_t ports_link;              /**< Link to ports list. */
    uint16_t port;                  /**< Bound port number. */

    mutex_t rx_lock;                /**< Receive queue lock. */
    condvar_t rx_cvar;              /**< Condition to wait for RX packets on. */
    uint32_t rx_size;               /**< Total amount of data in the RX queue. */
    list_t rx_queue;                /**< List of received packets. */
} udp_socket_t;

DEFINE_CLASS_CAST(udp_socket, net_socket, net);

/** UDP port address space (IPv4 and IPv6 have a different address space) */
typedef struct udp_port_space {
    rwlock_t lock;

    // TODO: Replace this with a hash table.
    list_t sockets;                 /**< List of all sockets bound to a port. */

    uint16_t next_ephemeral_port;   /**< Next ephemeral port number. */
} udp_port_space_t;

#define UDP_PORT_SPACE_INITIALIZER(_var) \
    { \
        .lock                = RWLOCK_INITIALIZER(_var.lock, "udp_ports_lock"), \
        .sockets             = LIST_INITIALIZER(_var.sockets), \
        .next_ephemeral_port = UDP_EPHEMERAL_PORT_FIRST, \
    }

static udp_port_space_t udp_ipv4_space = UDP_PORT_SPACE_INITIALIZER(udp_ipv4_space);
static udp_port_space_t udp_ipv6_space = UDP_PORT_SPACE_INITIALIZER(udp_ipv6_space);

static udp_port_space_t *get_socket_port_space(udp_socket_t *socket) {
    return (socket->net.socket.family == AF_INET6) ? &udp_ipv6_space : &udp_ipv4_space;
}

static udp_port_space_t *get_packet_port_space(net_packet_t *packet) {
    return (packet->type == NET_PACKET_TYPE_IPV6) ? &udp_ipv6_space : &udp_ipv4_space;
}

/** Finds the socket bound to the given port number (if any). */
static udp_socket_t *find_socket(udp_port_space_t *space, uint16_t port) {
    // TODO: Really needs a hash table...
    list_foreach(&space->sockets, iter) {
        udp_socket_t *socket = list_entry(iter, udp_socket_t, ports_link);
        if (socket->port == port)
            return socket;
    }

    return NULL;
}

/** Allocates a new ephemeral port number. */
static status_t alloc_ephemeral_port(udp_socket_t *socket) {
    assert(list_empty(&socket->ports_link));
    assert(socket->port == 0);

    udp_port_space_t *space = get_socket_port_space(socket);
    rwlock_write_lock(&space->lock);

    /* Round-robin allocation of port numbers. */
    uint16_t start = space->next_ephemeral_port;
    do {
        if (!find_socket(space, space->next_ephemeral_port)) {
            socket->port = space->next_ephemeral_port;
            list_append(&space->sockets, &socket->ports_link);
        }

        if (space->next_ephemeral_port == UDP_EPHEMERAL_PORT_LAST) {
            space->next_ephemeral_port = UDP_EPHEMERAL_PORT_FIRST;
        } else {
            space->next_ephemeral_port++;
        }
    } while (socket->port == 0 && space->next_ephemeral_port != start);

    rwlock_unlock(&space->lock);
    return (socket->port != 0) ? STATUS_SUCCESS : STATUS_TRY_AGAIN;
}

static void udp_socket_close(socket_t *_socket) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    /* Free the port. */
    if (socket->port != 0) {
        udp_port_space_t *space = get_socket_port_space(socket);
        rwlock_write_lock(&space->lock);

        list_remove(&socket->ports_link);

        rwlock_unlock(&space->lock);
    }

    mutex_lock(&socket->rx_lock);

    /* Empty anything in the RX queue. */
    while (!list_empty(&socket->rx_queue)) {
        udp_rx_packet_t *rx = list_first(&socket->rx_queue, udp_rx_packet_t, link);
        list_remove(&rx->link);
        kfree(rx);
    }

    mutex_unlock(&socket->rx_lock);

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
        goto out;

    /* Calculate a route for the packet. */
// TODO: For sockets bound to a specific address use that source.
    uint32_t interface_id;
    sockaddr_ip_t source_addr;
    ret = net_socket_route(&socket->net, (const sockaddr_t *)dest_addr, &interface_id, (sockaddr_t *)&source_addr);
    if (ret != STATUS_SUCCESS)
        goto out;

    /* Allocate an ephemeral port if we're not already bound. */
    if (socket->port == 0) {
        ret = alloc_ephemeral_port(socket);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    /* Initialise header. */
    header->length      = cpu_to_net16(packet_size);
    header->dest_port   = dest_addr->port;
    header->source_port = cpu_to_net16(socket->port);
    header->checksum    = 0;

    /* Calculate checksum based on header with checksum initialised to 0. */
    header->checksum = udp_checksum(header, packet_size, &source_addr, dest_addr);

    ret = net_socket_transmit(&socket->net, packet, interface_id, (sockaddr_t *)&source_addr, (const sockaddr_t *)dest_addr);
    if (ret == STATUS_SUCCESS)
        request->transferred += request->total;

out:
    net_packet_release(packet);
    return ret;
}

static status_t udp_socket_receive(
    socket_t *_socket, io_request_t *request, int flags, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));
    status_t ret;

    mutex_lock(&socket->rx_lock);

    /* Wait for a packet. */
    while (list_empty(&socket->rx_queue)) {
        ret = condvar_wait_etc(&socket->rx_cvar, &socket->rx_lock, -1, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS) {
            mutex_unlock(&socket->rx_lock);
            return ret;
        }
    }

    /* For UDP we only ever receive a maximum of 1 packet per receive call. If
     * the packet is larger than the requested size, the rest of the data is
     * lost. */
    udp_rx_packet_t *rx __cleanup_kfree = list_first(&socket->rx_queue, udp_rx_packet_t, link);
    list_remove(&rx->link);

    socket->rx_size -= rx->size;

    mutex_unlock(&socket->rx_lock);

    net_socket_addr_copy(&socket->net, (const sockaddr_t *)&rx->source_addr, max_addr_len, _addr, _addr_len);

    size_t copy_size = min(request->total, rx->size);
    return io_request_copy(request, rx->data, copy_size, true);
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

    list_init(&socket->ports_link);
    mutex_init(&socket->rx_lock, "udp_rx_lock", 0);
    condvar_init(&socket->rx_cvar, "udp_rx_cvar");
    list_init(&socket->rx_queue);

    socket->net.socket.ops = &udp_socket_ops;
    socket->net.protocol   = IPPROTO_UDP;
    socket->port           = 0;
    socket->rx_size        = 0;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}

/** Handles a received UDP packet. */
void udp_receive(net_packet_t *packet, const sockaddr_ip_t *source_addr, const sockaddr_ip_t *dest_addr) {
    const udp_header_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (!header) {
        dprintf("udp: dropping packet: too short for header\n");
        return;
    }

    uint16_t total_size  = net16_to_cpu(header->length);
    uint16_t dest_port   = net16_to_cpu(header->dest_port);

    if (total_size > packet->size) {
        dprintf("udp: dropping packet: header length is too long (header: %" PRIu16 ", packet: %" PRIu32 ")\n", total_size, packet->size);
        return;
    }

    // TODO: Validate checksum.
    if (header->checksum != 0) {

    }

    /* Look for the socket. */
    udp_port_space_t *space = get_packet_port_space(packet);
    rwlock_read_lock(&space->lock);

    udp_socket_t *socket = find_socket(space, dest_port);
    if (socket)
        mutex_lock(&socket->rx_lock);

    // TODO: What happens for sockets bound to a specific IP address? Do we
    // need to check the destination IP address?

    rwlock_unlock(&space->lock);

    if (!socket) {
        dprintf("udp: dropping packet: destination port not bound (%" PRIu16 ")\n", dest_port);
        return;
    }

    uint16_t data_size = total_size - sizeof(*header);

    /* Check for space in the receive queue. */
    if (socket->rx_size + data_size <= UDP_RX_QUEUE_MAX_SIZE) {
        /* We make a copy of the packet data, because the net_packet_t could
         * refer to space in the device receive buffer which is of limited size
         * shared across the whole system, and we have no idea how long this
         * will sit in the receive buffer. */
        // TODO: Don't copy if this is not a device buffer.
        // TODO: If there is a waiter, we could hand over a device buffer to it
        // directly without copying.
        udp_rx_packet_t *rx = kmalloc(sizeof(*rx) + data_size, MM_KERNEL);

        memcpy(&rx->source_addr, source_addr, sizeof(rx->source_addr));
        rx->source_addr.port = header->source_port;
        rx->size             = data_size;

        net_packet_copy_from(packet, rx->data, sizeof(*header), rx->size);

        list_init(&rx->link);
        list_append(&socket->rx_queue, &rx->link);

        socket->rx_size += rx->size;

        condvar_signal(&socket->rx_cvar);
    } else {
        dprintf("udp: dropping packet: socket %" PRIu16 " receive buffer is full\n", dest_port);
    }

    mutex_unlock(&socket->rx_lock);
}
