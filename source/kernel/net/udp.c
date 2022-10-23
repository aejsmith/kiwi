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
 * @brief               UDP protocol implementation.
 */

#include <mm/malloc.h>
#include <mm/page.h>

#include <io/request.h>

#include <net/interface.h>
#include <net/ip.h>
#include <net/ipv4.h>
#include <net/ipv6.h>
#include <net/packet.h>
#include <net/port.h>
#include <net/route.h>
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

    mutex_t lock;                   /**< Lock for the socket. */
    net_port_t port;                /**< Port allocation. */
    condvar_t rx_cvar;              /**< Condition to wait for RX packets on. */
    uint32_t rx_size;               /**< Total amount of data in the RX queue. */
    list_t rx_queue;                /**< List of received packets. */
    notifier_t rx_notifier;         /**< Notifier for RX packets. */
} udp_socket_t;

DEFINE_CLASS_CAST(udp_socket, net_socket, net);

static net_port_space_t udp_ipv4_space = NET_PORT_SPACE_INITIALIZER(udp_ipv4_space);
static net_port_space_t udp_ipv6_space = NET_PORT_SPACE_INITIALIZER(udp_ipv6_space);

static net_port_space_t *get_socket_port_space(udp_socket_t *socket) {
    return (socket->net.socket.family == AF_INET6) ? &udp_ipv6_space : &udp_ipv4_space;
}

static net_port_space_t *get_packet_port_space(net_packet_t *packet) {
    return (packet->type == NET_PACKET_TYPE_IPV6) ? &udp_ipv6_space : &udp_ipv4_space;
}

/** Finds the socket bound to a given number, if any, and takes its lock. */
static udp_socket_t *find_socket(net_packet_t *packet, uint16_t num) {
    net_port_space_t *space = get_packet_port_space(packet);
    net_port_space_read_lock(space);

    udp_socket_t *socket = NULL;
    net_port_t *port = net_port_lookup_unsafe(space, num);
    if (port) {
        socket = container_of(port, udp_socket_t, port);
        mutex_lock(&socket->lock);
    }

    net_port_space_unlock(space);
    return socket;
}

static status_t alloc_ephemeral_port(udp_socket_t *socket) {
    net_port_space_t *space = get_socket_port_space(socket);
    return net_port_alloc_ephemeral(space, &socket->port);
}

static void udp_socket_close(socket_t *_socket) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    net_port_space_t *space = get_socket_port_space(socket);
    net_port_free(space, &socket->port);

    mutex_lock(&socket->lock);

    /* Empty anything in the RX queue. */
    while (!list_empty(&socket->rx_queue)) {
        udp_rx_packet_t *rx = list_first(&socket->rx_queue, udp_rx_packet_t, link);
        list_remove(&rx->link);
        kfree(rx);
    }

    mutex_unlock(&socket->lock);

    kfree(socket);
}

static status_t udp_socket_wait(socket_t *_socket, object_event_t *event) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    status_t ret = STATUS_SUCCESS;
    switch (event->event) {
        case FILE_EVENT_READABLE:
            if (!list_empty(&socket->rx_queue)) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&socket->rx_notifier, object_event_notifier, event);
            }

            break;

        case FILE_EVENT_WRITABLE:
            // TODO: Change this when we add transmit buffering.
            object_event_signal(event, 0);
            break;

        default:
            ret = STATUS_INVALID_EVENT;
            break;
    }

    return ret;
}

static void udp_socket_unwait(socket_t *_socket, object_event_t *event) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    switch (event->event) {
        case FILE_EVENT_READABLE:
            notifier_unregister(&socket->rx_notifier, object_event_notifier, event);
            break;
    }
}

static status_t udp_socket_bind(socket_t *_socket, const sockaddr_t *addr, socklen_t addr_len) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));
    status_t ret;

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    ret = net_socket_addr_valid(&socket->net, addr, addr_len);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Already bound. */
    if (socket->port.num != 0)
        return STATUS_INVALID_ARG;

    net_port_space_t *space = get_socket_port_space(socket);

    /* We only support binding any address for now. */
    const sockaddr_ip_t *ip_addr = (const sockaddr_ip_t *)addr;
    if (ip_addr->family == AF_INET && ip_addr->ipv4.sin_addr.val == INADDR_ANY) {
        if (ip_addr->ipv4.sin_port != 0) {
            ret = net_port_alloc(space, &socket->port, net16_to_cpu(ip_addr->ipv4.sin_port));
        } else {
            ret = net_port_alloc_ephemeral(space, &socket->port);
            if (ret == STATUS_TRY_AGAIN) {
                /* This should be returned for ephemeral port allocation failure
                 * here. */
                ret = STATUS_ADDR_IN_USE;
            }
        }
    } else {
        ret = STATUS_NOT_SUPPORTED;
    }

    return ret;
}

static status_t udp_socket_send(
    socket_t *_socket, io_request_t *request, int flags, const sockaddr_t *addr,
    socklen_t addr_len)
{
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));
    status_t ret;

    mutex_lock(&socket->lock);

    const sockaddr_ip_t *dest_addr;
    if (addr_len == 0) {
        // TODO: connect() should be able to set a default address if this is NULL.
        ret = STATUS_DEST_ADDR_REQUIRED;
        goto err_unlock;
    } else {
        ret = net_socket_addr_valid(&socket->net, addr, addr_len);
        if (ret != STATUS_SUCCESS)
            goto err_unlock;

        dest_addr = (const sockaddr_ip_t *)addr;
    }

    /* Check packet size. */
    size_t packet_size = sizeof(udp_header_t) + request->total;
    if (packet_size > UDP_MAX_PACKET_SIZE || packet_size > socket->net.family->mtu) {
        ret = STATUS_MSG_TOO_LONG;
        goto err_unlock;
    }

    udp_header_t *header;
    net_packet_t *packet = net_packet_kmalloc(packet_size, MM_USER, (void **)&header);
    if (!packet) {
        ret = STATUS_NO_MEMORY;
        goto err_unlock;
    }

    void *data = &header[1];
    ret = io_request_copy(request, data, request->total, false);
    if (ret != STATUS_SUCCESS)
        goto err_release;

    /* Calculate a route for the packet. */
    // TODO: For sockets bound to a specific address use that source. We don't
    // support address binding yet.
    net_route_t route;
    ret = net_socket_route(&socket->net, (const sockaddr_t *)dest_addr, &route);
    if (ret != STATUS_SUCCESS)
        goto err_release;

    /* Allocate an ephemeral port if we're not already bound. */
    if (socket->port.num == 0) {
        ret = alloc_ephemeral_port(socket);
        if (ret != STATUS_SUCCESS)
            goto err_release;
    }

    /* Initialise header. */
    header->length      = cpu_to_net16(packet_size);
    header->dest_port   = dest_addr->port;
    header->source_port = cpu_to_net16(socket->port.num);
    header->checksum    = 0;

    mutex_unlock(&socket->lock);

    /* Calculate checksum based on header with checksum initialised to 0. */
    header->checksum = ip_checksum_pseudo(header, packet_size, IPPROTO_UDP, &route.source_addr, &route.dest_addr);

    /* 0 in the header indicates that no checksum has been calculated. */
    if (header->checksum == 0)
        header->checksum = 0xffff;

    ret = net_socket_transmit(&socket->net, packet, &route);
    if (ret == STATUS_SUCCESS)
        request->transferred += request->total;

    net_packet_release(packet);
    return ret;

err_release:
    net_packet_release(packet);

err_unlock:
    mutex_unlock(&socket->lock);
    return ret;
}

static status_t udp_socket_receive(
    socket_t *_socket, io_request_t *request, int flags, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));
    status_t ret;

    mutex_lock(&socket->lock);

    /* Wait for a packet. */
    while (list_empty(&socket->rx_queue)) {
        ret = condvar_wait_etc(&socket->rx_cvar, &socket->lock, -1, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS) {
            mutex_unlock(&socket->lock);
            return ret;
        }
    }

    /* For UDP we only ever receive a maximum of 1 packet per receive call. If
     * the packet is larger than the requested size, the rest of the data is
     * lost. */
    udp_rx_packet_t *rx __cleanup_kfree = list_first(&socket->rx_queue, udp_rx_packet_t, link);
    list_remove(&rx->link);

    socket->rx_size -= rx->size;

    mutex_unlock(&socket->lock);

    net_socket_addr_copy(&socket->net, (const sockaddr_t *)&rx->source_addr, max_addr_len, _addr, _addr_len);

    size_t copy_size = min(request->total, rx->size);
    return io_request_copy(request, rx->data, copy_size, true);
}

static const socket_ops_t udp_socket_ops = {
    .close      = udp_socket_close,
    .wait       = udp_socket_wait,
    .unwait     = udp_socket_unwait,
    .bind       = udp_socket_bind,
    .send       = udp_socket_send,
    .receive    = udp_socket_receive,
    .getsockopt = net_socket_getsockopt,
    .setsockopt = net_socket_setsockopt,
};

/** Creates a UDP socket. */
status_t udp_socket_create(sa_family_t family, socket_t **_socket) {
    assert(family == AF_INET || family == AF_INET6);

    udp_socket_t *socket = kmalloc(sizeof(udp_socket_t), MM_KERNEL);

    net_port_init(&socket->port);
    mutex_init(&socket->lock, "udp_lock", 0);
    condvar_init(&socket->rx_cvar, "udp_rx_cvar");
    list_init(&socket->rx_queue);
    notifier_init(&socket->rx_notifier, socket);

    socket->net.socket.ops = &udp_socket_ops;
    socket->net.protocol   = IPPROTO_UDP;
    socket->rx_size        = 0;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}

/** Handles a received UDP packet. */
void udp_receive(net_packet_t *packet, const net_addr_t *source_addr, const net_addr_t *dest_addr) {
    const udp_header_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (!header) {
        dprintf("udp: dropping packet: too short for header\n");
        return;
    }

    uint16_t total_size = net16_to_cpu(header->length);
    uint16_t dest_port  = net16_to_cpu(header->dest_port);

    if (total_size > packet->size) {
        dprintf("udp: dropping packet: header length is too long (header: %" PRIu16 ", packet: %" PRIu32 ")\n", total_size, packet->size);
        return;
    }

    if (header->checksum != 0) {
        if (ip_checksum_packet_pseudo(packet, 0, total_size, IPPROTO_UDP, source_addr, dest_addr) != 0) {
            dprintf("udp: dropping packet: checksum failed\n");
            return;
        }
    }

    /* Look for the socket and lock it. */
    udp_socket_t *socket = find_socket(packet, dest_port);
    if (!socket) {
        dprintf("udp: dropping packet: destination port not bound (%" PRIu16 ")\n", dest_port);
        return;
    }

    // TODO: What happens for sockets bound to a specific IP address? Do we
    // need to check the destination IP address?

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

        /* This is copied directly to userspace, make sure it's clear. */
        memset(&rx->source_addr, 0, sizeof(rx->source_addr));

        if (source_addr->family == AF_INET6) {
            rx->source_addr.ipv6.sin6_addr.val.high = source_addr->ipv6.val.high;
            rx->source_addr.ipv6.sin6_addr.val.low  = source_addr->ipv6.val.low;
        } else {
            rx->source_addr.ipv4.sin_addr.val = source_addr->ipv4.val;
        }

        rx->source_addr.family = source_addr->family;
        rx->source_addr.port   = header->source_port;
        rx->size               = data_size;

        net_packet_copy_from(packet, rx->data, sizeof(*header), rx->size);

        list_init(&rx->link);
        list_append(&socket->rx_queue, &rx->link);

        socket->rx_size += rx->size;

        condvar_broadcast(&socket->rx_cvar);
        notifier_run(&socket->rx_notifier, NULL, false);
    } else {
        dprintf("udp: dropping packet: socket %" PRIu16 " receive buffer is full\n", dest_port);
    }

    mutex_unlock(&socket->lock);
}
