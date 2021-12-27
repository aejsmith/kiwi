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

#include <lib/random.h>

#include <mm/malloc.h>

#include <net/packet.h>
#include <net/port.h>
#include <net/tcp.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

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

    /**
     * Reference count. TCP socket structures may need to be kept alive past
     * their owning user-facing socket.
     */
    refcount_t count;

    mutex_t lock;                       /**< Lock for the socket. */
    net_port_t port;                    /**< Port allocation. */
    sockaddr_ip_t dest_addr;            /**< Destination address. */

    /** Current socket state. */
    enum {
        TCP_STATE_CLOSED,
        TCP_STATE_SYN_SENT,
        TCP_STATE_LISTEN,
        TCP_STATE_ESTABLISHED,
        // TODO
    } state;

    /** Sequence state. */
    uint32_t initial_tx_seq;            /**< Initial transmit sequence. */
    uint32_t tx_seq;                    /**< Next transmit sequence number. */
    uint32_t initial_rx_seq;            /**< Initial receive sequence. */
    uint32_t rx_seq;                    /**< Next receive sequence number. */

    condvar_t state_cvar;               /**< Condition to wait for state changes on. */
} tcp_socket_t;

DEFINE_CLASS_CAST(tcp_socket, net_socket, net);

/** TCP transmit packet structure. */
typedef struct tcp_tx_packet {
    /** Route information. */
    uint32_t interface_id;
    sockaddr_ip_t source_addr;

    /** Packet allocation. */
    net_packet_t *packet;
    tcp_header_t *header;
} tcp_tx_packet_t;

// TODO: Make these configurable.
#define TCP_SYN_RETRIES             5   /**< Number of retries for connection attempts. */
#define TCP_SYN_INITIAL_TIMEOUT     1   /**< Initial connection timeout (seconds), multiplied by 2 each retry. */

static net_port_space_t tcp_ipv4_space = NET_PORT_SPACE_INITIALIZER(tcp_ipv4_space);
static net_port_space_t tcp_ipv6_space = NET_PORT_SPACE_INITIALIZER(tcp_ipv6_space);

static void tcp_socket_retain(tcp_socket_t *socket) {
    refcount_inc(&socket->count);
}

static void tcp_socket_release(tcp_socket_t *socket) {
    if (refcount_dec(&socket->count) == 0) {
        assert(socket->port.num == 0);

        kfree(socket);
    }
}

static net_port_space_t *get_socket_port_space(tcp_socket_t *socket) {
    return (socket->net.socket.family == AF_INET6) ? &tcp_ipv6_space : &tcp_ipv4_space;
}

static net_port_space_t *get_packet_port_space(net_packet_t *packet) {
    return (packet->type == NET_PACKET_TYPE_IPV6) ? &tcp_ipv6_space : &tcp_ipv4_space;
}

/** Finds the socket bound to a given number, if any, and add a reference. */
static tcp_socket_t *find_socket(net_packet_t *packet, uint16_t num) {
    net_port_space_t *space = get_packet_port_space(packet);
    net_port_space_read_lock(space);

    tcp_socket_t *socket = NULL;
    net_port_t *port = net_port_lookup_unsafe(space, num);
    if (port) {
        socket = container_of(port, tcp_socket_t, port);
        tcp_socket_retain(socket);
    }

    net_port_space_unlock(space);
    return socket;
}

/** Allocates an ephemeral port number for a socket. */
static status_t alloc_ephemeral_port(tcp_socket_t *socket) {
    net_port_space_t *space = get_socket_port_space(socket);
    return net_port_alloc_ephemeral(space, &socket->port);
}

/** Frees the port allocated for a socket, if any. */
static void free_port(tcp_socket_t *socket) {
    net_port_space_t *space = get_socket_port_space(socket);
    net_port_free(space, &socket->port);
}

/** Allocates an initial sequence number for a socket. */
static void alloc_initial_tx_seq(tcp_socket_t *socket) {
    // TODO: https://datatracker.ietf.org/doc/html/rfc1948.html
    socket->initial_tx_seq = random_get32();
}

/**
 * Routes, allocates and initialises a packet to transmit. The header is filled
 * out with initial information from the socket, which can be adjusted as
 * needed. The packet is initially sized for the header, data can be appended
 * if needed.
 */
static status_t prepare_transmit_packet(tcp_socket_t *socket, tcp_tx_packet_t *packet) {
    status_t ret;

    // TODO: We could cache routes in the socket. Need to have some way to
    // identify when routing might have changed, e.g. a routing table version.
    ret = net_socket_route(
        &socket->net, (const sockaddr_t *)&socket->dest_addr,
        &packet->interface_id, (sockaddr_t *)&packet->source_addr);
    if (ret != STATUS_SUCCESS)
        return ret;

    tcp_header_t *header;
    packet->packet = net_packet_kmalloc(sizeof(*header), MM_KERNEL, (void **)&header);
    packet->header = header;

    header->source_port = cpu_to_net16(socket->port.num);
    header->dest_port   = socket->dest_addr.port;
    header->seq_num     = cpu_to_net32(socket->tx_seq);
    header->ack_num     = cpu_to_net32(socket->rx_seq);
    header->reserved    = 0;
    header->data_offset = sizeof(*header) / sizeof(uint32_t);
    header->flags       = TCP_ACK;
    header->window_size = cpu_to_net16(0xffff); // TODO
    header->checksum    = 0;
    header->urg_ptr     = 0;

    return STATUS_SUCCESS;
}

/** Checksums and transmits a previously prepared packet. */
static status_t transmit_packet(tcp_socket_t *socket, tcp_tx_packet_t *packet, bool release) {
    /* Checksum the packet based on checksum set to 0. */
    assert(packet->header->checksum == 0);
    packet->header->checksum = ip_checksum_packet_pseudo(
        packet->packet, 0, packet->packet->size, IPPROTO_TCP, &packet->source_addr,
        &socket->dest_addr);

    status_t ret = net_socket_transmit(
        &socket->net, packet->packet, packet->interface_id,
        (sockaddr_t *)&packet->source_addr, (const sockaddr_t *)&socket->dest_addr);

    if (release)
        net_packet_release(packet->packet);

    return ret;
}

/** Transmits an ACK for the current rx_seq value. */
static void transmit_ack(tcp_socket_t *socket) {
    tcp_tx_packet_t packet;
    status_t ret = prepare_transmit_packet(socket, &packet);
    if (ret == STATUS_SUCCESS)
        ret = transmit_packet(socket, &packet, true);

    if (ret != STATUS_SUCCESS) {
        /* This should be OK if this is just a temporary failure, we'll
         * re-acknowledge it later. */
        dprintf("tcp: failed to transmit ACK: %d\n", ret);
    }
}

static void tcp_socket_close(socket_t *_socket) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    // TODO: Send FIN packet. We should wait for this and not actually remove
    // the port until it's acknowledged or timed out.

    mutex_lock(&socket->lock);

    free_port(socket);

    // TODO. What cleanup should be done here vs tcp_socket_release().

    mutex_unlock(&socket->lock);

    tcp_socket_release(socket);
}

static status_t tcp_socket_connect(socket_t *_socket, const sockaddr_t *addr, socklen_t addr_len) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));
    status_t ret;

    ret = net_socket_addr_valid(&socket->net, addr, addr_len);
    if (ret != STATUS_SUCCESS)
        return ret;

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    if (socket->state != TCP_STATE_CLOSED) {
        return (socket->state == TCP_STATE_SYN_SENT)
            ? STATUS_ALREADY_IN_PROGRESS
            : STATUS_ALREADY_CONNECTED;
    }

    memcpy(&socket->dest_addr, addr, addr_len);

    /* We're in the closed state so we shouldn't have a port already. */
    assert(socket->port.num == 0);
    ret = alloc_ephemeral_port(socket);
    if (ret != STATUS_SUCCESS)
        return ret;

    alloc_initial_tx_seq(socket);

    socket->state = TCP_STATE_SYN_SENT;

    /* SYN retry loop. */
    unsigned retries = TCP_SYN_RETRIES;
    nstime_t timeout = secs_to_nsecs(TCP_SYN_INITIAL_TIMEOUT);
    ret = STATUS_SUCCESS;
    while (retries > 0 && socket->state != TCP_STATE_ESTABLISHED) {
        /* Retries are sent with the same sequence number. */
        socket->tx_seq = socket->initial_tx_seq;

        tcp_tx_packet_t packet;
        ret = prepare_transmit_packet(socket, &packet);
        if (ret != STATUS_SUCCESS)
            break;

        /* prepare_transmit_packet() assumes we're past the initial SYN,
         * override these. */
        packet.header->flags   = TCP_SYN;
        packet.header->ack_num = 0;

        /* Increment in case we succeed. */
        socket->tx_seq++;

        ret = transmit_packet(socket, &packet, true);
        if (ret != STATUS_SUCCESS)
            break;

        ret = condvar_wait_etc(&socket->state_cvar, &socket->lock, timeout, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS && ret != STATUS_TIMED_OUT)
            break;

        retries--;
        timeout *= 2;
    }

    if (socket->state == TCP_STATE_ESTABLISHED) {
        ret = STATUS_SUCCESS;
    } else {
        if (ret == STATUS_SUCCESS)
            ret = STATUS_TIMED_OUT;

        free_port(socket);
        socket->state = TCP_STATE_CLOSED;
    }

    return ret;
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

    tcp_socket_t *socket = kmalloc(sizeof(tcp_socket_t), MM_KERNEL | MM_ZERO);

    refcount_set(&socket->count, 1);
    mutex_init(&socket->lock, "tcp_socket_lock", 0);
    net_port_init(&socket->port);
    condvar_init(&socket->state_cvar, "tcp_socket_state_cvar");

    socket->net.socket.ops = &tcp_socket_ops;
    socket->net.protocol   = IPPROTO_TCP;
    socket->state          = TCP_STATE_CLOSED;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}

/** Handles packets while in the SYN_SENT state. */
static void receive_syn_sent(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    if ((header->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
        uint32_t seq_num = net32_to_cpu(header->seq_num);
        uint32_t ack_num = net32_to_cpu(header->ack_num);

        /* tx_seq is incremented after sending a SYN, so should be equal. */
        if (ack_num != socket->tx_seq) {
            dprintf("tcp: incorrect sequence number for SYN-ACK, dropping\n");
            return;
        }

        socket->initial_rx_seq = seq_num;
        socket->rx_seq         = seq_num + 1;

        transmit_ack(socket);

        socket->state = TCP_STATE_ESTABLISHED;
        condvar_broadcast(&socket->state_cvar);
    } else {
        // TODO: Do we need to handle SYN without ACK in this state? This would
        // be unexpected for a client socket.
        dprintf("tcp: unexpected non-SYN-ACK packet, dropping\n");
        return;
    }
}

/** Handles a received TCP packet. */
void tcp_receive(net_packet_t *packet, const sockaddr_ip_t *source_addr, const sockaddr_ip_t *dest_addr) {
    const tcp_header_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (!header) {
        dprintf("tcp: dropping packet: too short for header\n");
        return;
    }

    if (ip_checksum_packet_pseudo(packet, 0, packet->size, IPPROTO_TCP, source_addr, dest_addr) != 0) {
        dprintf("tcp: dropping packet: checksum failed\n");
        return;
    }

    /* Look for the socket. */
    uint16_t dest_port = net16_to_cpu(header->dest_port);
    tcp_socket_t *socket = find_socket(packet, dest_port);
    if (!socket) {
        // TODO: If this is a SYN, send RST?
        dprintf("tcp: dropping packet: destination port not bound (%" PRIu16 ")\n", dest_port);
        return;
    }

    mutex_lock(&socket->lock);

    /* Re-check port now that we've taken the lock in case it changed. */
    if (socket->port.num == dest_port) {
        assert(socket->state != TCP_STATE_CLOSED);

        dprintf("tcp: received packet\n");

        switch (socket->state) {
            case TCP_STATE_SYN_SENT:
                receive_syn_sent(socket, header, packet);
                break;
            default:
                break;
        }
    }

    mutex_unlock(&socket->lock);
    tcp_socket_release(socket);
}
