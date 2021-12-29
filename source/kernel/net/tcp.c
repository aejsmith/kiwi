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
 *
 * TODO:
 *  - Support SACK.
 */

#include <io/request.h>

#include <lib/notifier.h>
#include <lib/random.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <net/ipv4.h>
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

/**
 * TCP buffer structure. This implements a circular buffer for sending and
 * receiving data.
 */
typedef struct tcp_buffer {
    uint8_t *data;                      /**< Buffer data. */
    uint32_t start;                     /**< Start position. */
    uint32_t curr_size;                 /**< Number of bytes in buffer. */
    uint32_t max_size;                  /**< Maximum number of bytes in the buffer (power of 2). */
    condvar_t cvar;                     /**< Condition to wait for space (TX) or data (RX). */
    notifier_t notifier;                /**< Notifier to wait for space or data. */
} tcp_buffer_t;

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
        TCP_STATE_REFUSED,
        // TODO
    } state;

    /**
     * Transmit buffer and sequence state. The start of the buffer corresponds
     * to the tx_unack sequence number.
     */
    tcp_buffer_t tx_buffer;
    uint32_t initial_tx_seq;            /**< Initial transmit sequence. */
    uint32_t tx_seq;                    /**< Next transmit sequence number. */
    uint32_t tx_unack;                  /**< First unacknowledged transmit sequence number. */

    /** Receive buffer and sequence state. */
    tcp_buffer_t rx_buffer;
    uint32_t initial_rx_seq;            /**< Initial receive sequence. */
    uint32_t rx_seq;                    /**< Next receive sequence number. */

    condvar_t state_cvar;               /**< Condition to wait for state changes on. */
} tcp_socket_t;

DEFINE_CLASS_CAST(tcp_socket, net_socket, net);

/**
 * TCP transmit packet structure. This is just used to keep track of state
 * while sending a packet out to the network, it is not a persistent structure.
 */
typedef struct tcp_tx_packet {
    /** Route information. */
    uint32_t interface_id;
    sockaddr_ip_t source_addr;

    /** Packet allocation. */
    net_packet_t *packet;
    tcp_header_t *header;
} tcp_tx_packet_t;

/** TCP parameters. TODO: Make these configurable. */
enum {
    /** Number of retries for connection attempts. */
    TCP_SYN_RETRIES = 5,

    /** Initial connection timeout (seconds), multiplied by 2 each retry. */
    TCP_SYN_INITIAL_TIMEOUT = 1,

    /** TCP buffer sizes. */
    TCP_TX_BUFFER_SIZE = PAGE_SIZE,
    TCP_RX_BUFFER_SIZE = PAGE_SIZE,
};

static net_port_space_t tcp_ipv4_space = NET_PORT_SPACE_INITIALIZER(tcp_ipv4_space);
static net_port_space_t tcp_ipv6_space = NET_PORT_SPACE_INITIALIZER(tcp_ipv6_space);

static void tcp_socket_retain(tcp_socket_t *socket) {
    refcount_inc(&socket->count);
}

static void tcp_socket_release(tcp_socket_t *socket) {
    if (refcount_dec(&socket->count) == 0) {
        assert(socket->port.num == 0);

        assert(notifier_empty(&socket->tx_buffer.notifier));
        assert(notifier_empty(&socket->rx_buffer.notifier));

        kfree(socket->tx_buffer.data);
        kfree(socket->rx_buffer.data);
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
static status_t prepare_tx_packet(tcp_socket_t *socket, tcp_tx_packet_t *packet) {
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
    header->window_size = cpu_to_net16(0xffff); // TODO: Window size
    header->checksum    = 0;
    header->urg_ptr     = 0;

    return STATUS_SUCCESS;
}

/** Checksums and transmits a previously prepared packet. */
static status_t tx_packet(tcp_socket_t *socket, tcp_tx_packet_t *packet, bool release) {
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

/** Transmits an ACK packet for the current rx_seq value. */
static void tx_ack_packet(tcp_socket_t *socket) {
    tcp_tx_packet_t packet;
    status_t ret = prepare_tx_packet(socket, &packet);
    if (ret == STATUS_SUCCESS)
        ret = tx_packet(socket, &packet, true);

    if (ret != STATUS_SUCCESS) {
        // TODO: Routing or device error. Should close the socket?
        kprintf(LOG_WARN, "tcp: failed to transmit ACK: %d\n", ret);
    }
}

/**
 * Flushes the transmit buffer. This will retransmit unacknowledged segments if
 * we determine that it is time to do so, and transmit segments for any new data
 * that has been added to the buffer.
 */
static void flush_tx_buffer(tcp_socket_t *socket) {
    tcp_buffer_t *buffer = &socket->tx_buffer;
    status_t ret;

    // TODO: Retransmit segments due for retransmission. Make sure to set same
    // flags, sequence, etc.

    /* Calculate what we have in the buffer that has yet to be attempted at all.
     * Everything before tx_seq we have already tried sending at least once. */
    uint32_t unsent_size = (socket->tx_unack + buffer->curr_size) - socket->tx_seq;

    /* Calculate maximum segment size. */
    // TODO: This can be negotiated using MSS option.
    uint32_t mtu = socket->net.family_ops->mtu;
    // TODO: HACK: We don't implement fragmentation yet so sending larger than
    // device MTU will fail, but we don't have a device yet as we haven't
    // routed. Even once we implement fragmentation, it would be better to get
    // the device MTU to avoid fragmentation. Since we will probably implement
    // caching for routing, we could cache the MTU with the routing information.
    mtu = min(mtu, 1500 - sizeof(ipv4_header_t));
    uint32_t max_segment_size = mtu - sizeof(tcp_header_t);

    /* Divide the unsent data into segments. */
    while (unsent_size > 0) {
        uint32_t segment_size = min(unsent_size, max_segment_size);

        tcp_tx_packet_t packet;
        ret = prepare_tx_packet(socket, &packet);
        if (ret != STATUS_SUCCESS) {
            // TODO: This is a failure to route. Should we close the socket in
            // this situation?
            kprintf(LOG_WARN, "tcp: failed to route packet: %d\n", ret);
            break;
        }

        /* If this is the last segment we're going to send for now, set PSH. */
        if (segment_size == unsent_size)
            packet.header->flags |= TCP_PSH;

        /* Add the segment data. */
        // TODO: We're currently using external buffers here under the
        // assumption that the packet will be consumed by net_socket_transmit
        // and not live any longer. In future, we'll implement some packet
        // queueing for the situation where the device buffer is full. This will
        // need to be careful to ensure that underlying buffer data stays around
        // as long as any packets referring to it do - we'll need to be
        // particularly careful for how we handle removing buffer data upon ACK.
        // Device drivers may also want to keep packets around for zero-copy
        // transmit in future, which will also need consideration here.
        uint32_t pos = (buffer->start + buffer->curr_size - unsent_size) & (buffer->max_size - 1);
        if (pos + segment_size > buffer->max_size) {
            uint32_t split = buffer->max_size - pos;
            net_packet_append(packet.packet, net_buffer_from_external(&buffer->data[pos], split));
            net_packet_append(packet.packet, net_buffer_from_external(&buffer->data[0], segment_size - split));
        } else {
            net_packet_append(packet.packet, net_buffer_from_external(&buffer->data[pos], segment_size));
        }

        ret = tx_packet(socket, &packet, true);
        if (ret != STATUS_SUCCESS) {
            kprintf(LOG_WARN, "tcp: failed to transmit packet: %d\n", ret);
            break;
        }

        /* Advance sequence number. Done after transmitting, the header sequence
         * in the packet we transmit is the number of the first byte of data in
         * the packet. */
        socket->tx_seq += segment_size;
        unsent_size    -= segment_size;
    }
}

/**
 * Handles acknowledgement received from the remote end by clearing out data
 * from the transmit buffer that is no longer needed.
 */
static void ack_tx_buffer(tcp_socket_t *socket, uint32_t ack_num) {
    /*
     * Check that this ack is acceptable:
     *   A new acknowledgment (called an "acceptable ack"), is one for which
     *   the inequality below holds:
     *     SND.UNA < SEG.ACK =< SND.NXT
     */
    if (!TCP_SEQ_LE(socket->tx_unack, ack_num) || !TCP_SEQ_LE(ack_num, socket->tx_seq)) {
        dprintf("tcp: received unexpected ACK sequence, ignoring\n");
        return;
    }

    uint32_t ack_size = ack_num - socket->tx_unack;

    if (ack_size > 0) {
        tcp_buffer_t *buffer = &socket->tx_buffer;

        assert(ack_size <= buffer->curr_size);

        // TODO: Handle anything to do with retransmission necessary here
        // (cancel timers, drop segment info).

        socket->tx_unack   = ack_num;
        buffer->start      = (buffer->start + ack_size) & (buffer->max_size - 1);
        buffer->curr_size -= ack_size;

        condvar_broadcast(&buffer->cvar);
        notifier_run(&buffer->notifier, NULL, false);
    }
}

static void tcp_socket_close(socket_t *_socket) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    // TODO: Send FIN packet. We should wait for this and not actually remove
    // the port until it's acknowledged or timed out.

    mutex_lock(&socket->lock);

    /* If the handle is being closed, there shouldn't be any waiters on it as
     * they'd hold a reference to the handle. */
    assert(list_empty(&socket->state_cvar.threads));

    free_port(socket);
    socket->state = TCP_STATE_CLOSED;
    // TODO. What cleanup should be done here vs tcp_socket_release().

    mutex_unlock(&socket->lock);

    tcp_socket_release(socket);
}

static status_t tcp_socket_wait(socket_t *_socket, object_event_t *event) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    status_t ret = STATUS_SUCCESS;
    switch (event->event) {
        case FILE_EVENT_READABLE:
            if (socket->rx_buffer.curr_size > 0) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&socket->rx_buffer.notifier, object_event_notifier, event);
            }

            break;

        case FILE_EVENT_WRITABLE:
            if (socket->tx_buffer.curr_size < socket->tx_buffer.max_size) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&socket->tx_buffer.notifier, object_event_notifier, event);
            }

            break;

        default:
            ret = STATUS_INVALID_EVENT;
            break;
    }

    return ret;
}

static void tcp_socket_unwait(socket_t *_socket, object_event_t *event) {
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    switch (event->event) {
        case FILE_EVENT_READABLE:
            notifier_unregister(&socket->rx_buffer.notifier, object_event_notifier, event);
            break;

        case FILE_EVENT_WRITABLE:
            notifier_unregister(&socket->tx_buffer.notifier, object_event_notifier, event);
            break;
    }
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
    while (retries > 0 && socket->state == TCP_STATE_SYN_SENT) {
        /* Retries are sent with the same sequence number. */
        socket->tx_seq   = socket->initial_tx_seq;
        socket->tx_unack = socket->initial_tx_seq;

        tcp_tx_packet_t packet;
        ret = prepare_tx_packet(socket, &packet);
        if (ret != STATUS_SUCCESS)
            break;

        /* prepare_tx_packet() assumes we're past the initial SYN, override
         * these. */
        packet.header->flags   = TCP_SYN;
        packet.header->ack_num = 0;

        /* Increment in case we succeed. */
        socket->tx_seq++;
        socket->tx_unack = socket->tx_seq;

        ret = tx_packet(socket, &packet, true);
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
        if (ret == STATUS_SUCCESS) {
            ret = (socket->state == TCP_STATE_REFUSED)
                ? STATUS_CONNECTION_REFUSED
                : STATUS_TIMED_OUT;
        }

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
    status_t ret;

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    if (socket->state != TCP_STATE_ESTABLISHED)
        return STATUS_NOT_CONNECTED;

    if (addr_len > 0)
        return STATUS_ALREADY_CONNECTED;

    ret = STATUS_SUCCESS;

    /*
     * Copy the data into our transmit buffer. We don't need to indicate
     * whether anything was actually successfully sent upon return from this,
     * so the transferred amount we return is just what's copied into the
     * buffer.
     */
    tcp_buffer_t *buffer = &socket->tx_buffer;
    while (request->transferred < request->total) {
        size_t remaining = request->total - request->transferred;
        uint32_t space   = buffer->max_size - buffer->curr_size;
        uint32_t size    = min(remaining, (size_t)space);

        if (size == 0) {
            /* We need to wait for some space to become available. Flush
             * anything we've added and wait. */
            flush_tx_buffer(socket);

            ret = condvar_wait_etc(&buffer->cvar, &socket->lock, -1, SLEEP_INTERRUPTIBLE);
            if (ret != STATUS_SUCCESS)
                break;

            /* Check that we're still connected after waiting for space. */
            if (socket->state != TCP_STATE_ESTABLISHED) {
                ret = STATUS_NOT_CONNECTED;
                break;
            }

            continue;
        }

        uint32_t pos = (buffer->start + buffer->curr_size) & (buffer->max_size - 1);
        if (pos + size > buffer->max_size) {
            /* Straddles the end of the circular buffer, split into 2 copies. */
            uint32_t split = buffer->max_size - pos;
            ret = io_request_copy(request, &buffer->data[pos], split, true);
            if (ret == STATUS_SUCCESS) {
                ret = io_request_copy(request, &buffer->data[0], size - split, true);
                if (ret != STATUS_SUCCESS) {
                    /* Don't do a partial transfer in the copy fail case. */
                    request->transferred -= split;
                }
            }
        } else {
            ret = io_request_copy(request, &buffer->data[pos], size, true);
        }

        if (ret == STATUS_SUCCESS) {
            buffer->curr_size += size;
        } else {
            break;
        }
    }

    /* Flush anything we've added to the buffer. */
    flush_tx_buffer(socket);

    return STATUS_SUCCESS;
}

static status_t tcp_socket_receive(
    socket_t *_socket, io_request_t *request, int flags, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    tcp_socket_t *socket = cast_tcp_socket(cast_net_socket(_socket));
    status_t ret;

    MUTEX_SCOPED_LOCK(lock, &socket->lock);

    tcp_buffer_t *buffer = &socket->rx_buffer;

    /* Wait for any data to be available on the socket. */
    // TODO: If this changes when we implement data reordering, make sure to
    // update tcp_socket_wait.
    while (buffer->curr_size == 0) {
        if (socket->state != TCP_STATE_ESTABLISHED)
            return STATUS_NOT_CONNECTED;

        ret = condvar_wait_etc(&buffer->cvar, &socket->lock, -1, SLEEP_INTERRUPTIBLE);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    assert(request->transferred == 0);
    uint32_t size = min((size_t)buffer->curr_size, request->total);

    if (buffer->start + size > buffer->max_size) {
        /* Straddles the end of the circular buffer, split into 2 copies. */
        uint32_t split = buffer->max_size - buffer->start;
        ret = io_request_copy(request, &buffer->data[buffer->start], split, true);
        if (ret == STATUS_SUCCESS) {
            ret = io_request_copy(request, &buffer->data[0], size - split, true);
            if (ret != STATUS_SUCCESS) {
                /* Don't do a partial transfer in the copy fail case. */
                request->transferred -= split;
            }
        }
    } else {
        ret = io_request_copy(request, &buffer->data[buffer->start], size, true);
    }

    if (ret != STATUS_SUCCESS)
        return ret;

    buffer->start      = (buffer->start + size) & (buffer->max_size - 1);
    buffer->curr_size -= size;

    net_socket_addr_copy(&socket->net, (const sockaddr_t *)&socket->dest_addr, max_addr_len, _addr, _addr_len);
    return STATUS_SUCCESS;
}

static const socket_ops_t tcp_socket_ops = {
    .close   = tcp_socket_close,
    .wait    = tcp_socket_wait,
    .unwait  = tcp_socket_unwait,
    .connect = tcp_socket_connect,
    .send    = tcp_socket_send,
    .receive = tcp_socket_receive,
};

static bool tcp_buffer_init(tcp_buffer_t *buffer, uint32_t size, const char *name) {
    assert(is_pow2(size));

    buffer->data = kmalloc(size, MM_USER);
    if (!buffer->data)
        return false;

    condvar_init(&buffer->cvar, name);
    notifier_init(&buffer->notifier, buffer);

    buffer->start     = 0;
    buffer->curr_size = 0;
    buffer->max_size  = size;

    return true;
}

/** Creates a TCP socket. */
status_t tcp_socket_create(sa_family_t family, socket_t **_socket) {
    assert(family == AF_INET || family == AF_INET6);

    tcp_socket_t *socket = kmalloc(sizeof(tcp_socket_t), MM_KERNEL | MM_ZERO);

    refcount_set(&socket->count, 1);
    mutex_init(&socket->lock, "tcp_socket_lock", 0);
    net_port_init(&socket->port);
    condvar_init(&socket->state_cvar, "tcp_socket_state");

    socket->net.socket.ops = &tcp_socket_ops;
    socket->net.protocol   = IPPROTO_TCP;
    socket->state          = TCP_STATE_CLOSED;

    if (!tcp_buffer_init(&socket->tx_buffer, TCP_TX_BUFFER_SIZE, "tcp_tx_buffer")) {
        kfree(socket);
        return STATUS_NO_MEMORY;
    }

    if (!tcp_buffer_init(&socket->rx_buffer, TCP_RX_BUFFER_SIZE, "tcp_rx_buffer")) {
        kfree(socket->tx_buffer.data);
        kfree(socket);
        return STATUS_NO_MEMORY;
    }

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

        tx_ack_packet(socket);

        socket->state = TCP_STATE_ESTABLISHED;
        condvar_broadcast(&socket->state_cvar);
    } else if (header->flags & TCP_RST) {
        socket->state = TCP_STATE_REFUSED;
        condvar_broadcast(&socket->state_cvar);
    } else {
        // TODO: Do we need to handle SYN without ACK in this state? This would
        // be unexpected for a client socket.
        dprintf("tcp: unexpected packet in SYN_SENT state, dropping\n");
        return;
    }
}

/** Handles packets while in the ESTABLISHED state. */
static void receive_established(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    /* Start by handling acknowledgement. Any packet received in this state
     * should have ACK set. */
    if (!(header->flags & TCP_ACK)) {
        // TODO: Close connection on error?
        dprintf("tcp: packet received in ESTABLISHED state does not have ACK set, dropping\n");
        return;
    }

    ack_tx_buffer(socket, net32_to_cpu(header->ack_num));

    /* data_offset is validated by tcp_receive(). */
    uint32_t data_offset = header->data_offset * sizeof(uint32_t);
    uint32_t data_size   = packet->size - data_offset;
    if (data_size > 0) {
        uint32_t seq_num  = net32_to_cpu(header->seq_num);

        /* We can accept data if the start sequence is equal to rx_seq (next
         * that we're expecting), or it is less than rx_seq and seq_next is
         * greater than rx_seq. */
        // TODO: Accept data with a start sequence greater than what we are
        // expecting - this can happen if packets arrive out of order. We'll
        // need to keep track of segments that we've received so that we can
        // know once we've got contiguous data. For now, we'll rely on
        // retransmission if we get things out of order.
        if (seq_num != socket->rx_seq) {
            uint32_t seq_next = seq_num + data_size;
            if (TCP_SEQ_LT(seq_num, socket->rx_seq) && TCP_SEQ_GT(seq_next, socket->rx_seq)) {
                uint32_t diff = socket->rx_seq - seq_num;
                data_offset += diff;
                data_size   -= diff;
            } else {
                /* Unexpected, drop. */
                dprintf("tcp: dropping unexpected segment with seq %" PRIu32 " (expecting %" PRIu32 ")\n", seq_num, socket->rx_seq);
                data_size = 0;
            }
        }

        tcp_buffer_t *buffer = &socket->rx_buffer;

        /* Clamp by what we can fit in the buffer. */
        uint32_t space = buffer->max_size - buffer->curr_size;
        if (data_size > 0 && data_size > space) {
            dprintf("tcp: RX buffer full, dropping data (received %" PRIu32 " bytes, accepting %" PRIu32 ")\n", data_size, space);
            data_size = space;
        }

        if (data_size > 0) {
            uint32_t pos = (buffer->start + buffer->curr_size) & (buffer->max_size - 1);
            if (pos + data_size > buffer->max_size) {
                /* Straddles the end of the circular buffer, split into 2 copies. */
                uint32_t split = buffer->max_size - pos;
                net_packet_copy_from(packet, &buffer->data[pos], data_offset, split);
                net_packet_copy_from(packet, &buffer->data[0], data_offset + split, data_size - split);
            } else {
                net_packet_copy_from(packet, &buffer->data[pos], data_offset, data_size);
            }

            socket->rx_seq    += data_size;
            buffer->curr_size += data_size;

            condvar_broadcast(&buffer->cvar);
            notifier_run(&buffer->notifier, NULL, false);
        }

        /* Acknowledge what we've accepted (if anything). */
        tx_ack_packet(socket);
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

    if (header->data_offset * sizeof(uint32_t) > packet->size) {
        dprintf("tcp: dropping packet: data offset is invalid\n");
        return;
    }

    /* Look for the socket. */
    uint16_t dest_port = net16_to_cpu(header->dest_port);
    tcp_socket_t *socket = find_socket(packet, dest_port);
    if (!socket) {
        // TODO: Send RST? For SYN only?
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
            case TCP_STATE_ESTABLISHED:
                receive_established(socket, header, packet);
                break;
            default:
                break;
        }
    }

    mutex_unlock(&socket->lock);
    tcp_socket_release(socket);
}
