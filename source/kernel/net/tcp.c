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
#include <net/route.h>
#include <net/tcp.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <assert.h>
#include <kdb.h>
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

/** TCP socket state. Order of this is important! */
typedef enum tcp_state {
    /** Socket is closed, has never been opened. */
    TCP_STATE_CLOSED,

    /** Internal: Remote host refused our connection. */
    TCP_STATE_REFUSED,

    /** Connecting states. */
    TCP_STATE_SYN_SENT,
    TCP_STATE_LISTEN,

    TCP_STATE_ESTABLISHED,

    /** Closing states. */
    TCP_STATE_CLOSE_ACTIVE,             /**< Internal: Initiated active close. */
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,

    /**
     * Internal: We've closed a connection that was previously open (due to the remote
     * host closing it), but the socket has not been expliclitly closed by its
     * owner.
     */
    TCP_STATE_CLOSE_COMPLETE,
} tcp_state_t;

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
    tcp_state_t state;                  /**< Current socket state. */

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
    timer_t close_timer;                /**< Close timeout. */
} tcp_socket_t;

DEFINE_CLASS_CAST(tcp_socket, net_socket, net);

/**
 * TCP transmit packet structure. This is just used to keep track of state
 * while sending a packet out to the network, it is not a persistent structure.
 */
typedef struct tcp_tx_packet {
    net_route_t route;
    net_packet_t *packet;
    tcp_header_t *header;
} tcp_tx_packet_t;

/** TCP parameters. TODO: Make these configurable. */
enum {
    /** Number of retries for connection attempts. */
    TCP_SYN_RETRIES = 5,

    /** Initial connection timeout (seconds), multiplied by 2 each retry. */
    TCP_SYN_INITIAL_TIMEOUT = 1,

    /**
     * Timeout for connection closing after which the connection will be
     * forcibly closed. This is also used for waiting in the TIME_WAIT state for
     * the normal close procedure.
     *
     * The TIME_WAIT timeout is defined by the TCP RFC as 2 * MSL = 4 minutes,
     * but we follow Linux's default and use 1 minute, as 4 is quite
     * overzealous.
     */
    TCP_CLOSE_TIMEOUT = 60,

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
        bool __unused is_closed_state =
            socket->state == TCP_STATE_CLOSED ||
            socket->state == TCP_STATE_CLOSE_COMPLETE;
        assert(is_closed_state);

        dprintf("tcp: freeing socket %p\n", socket);

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
    ret = net_socket_route(&socket->net, (const sockaddr_t *)&socket->dest_addr, &packet->route);
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
        packet->packet, 0, packet->packet->size, IPPROTO_TCP, &packet->route.source_addr,
        &packet->route.dest_addr);

    status_t ret = net_socket_transmit(&socket->net, packet->packet, &packet->route);

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

static inline bool is_closing(tcp_socket_t *socket) {
    return socket->state >= TCP_STATE_CLOSE_ACTIVE && socket->state < TCP_STATE_CLOSE_COMPLETE;
}

static void start_close(tcp_socket_t *socket, tcp_state_t new_state) {
    assert(!is_closing(socket));

    /* This keeps the socket alive until we either complete the close or until
     * it times out. */
    tcp_socket_retain(socket);
    timer_start(&socket->close_timer, secs_to_nsecs(TCP_CLOSE_TIMEOUT), TIMER_ONESHOT);

    socket->state = new_state;
    assert(is_closing(socket));
}

static void finish_close(tcp_socket_t *socket, tcp_state_t new_state, bool is_timer) {
    if (is_closing(socket) && !is_timer) {
        /* timer_stop() may need to wait for a handler execution to complete,
         * which would want to take the lock. */
        mutex_unlock(&socket->lock);

        uint32_t exec_count = timer_stop(&socket->close_timer);
        assert(exec_count == 0 || exec_count == 1);

        mutex_lock(&socket->lock);

        /* The handler will have released its reference if it executed,
         * otherwise we must drop the reference added by start_close(). */
        if (exec_count == 0)
            tcp_socket_release(socket);
    }

    dprintf("tcp: %" PRIu16 ": closed\n", socket->port.num);

    free_port(socket);

    socket->state = new_state;
    assert(!is_closing(socket));
}

static bool close_timer_func(void *_socket) {
    tcp_socket_t *socket = _socket;

    mutex_lock(&socket->lock);

    if (is_closing(socket)) {
        if (socket->state == TCP_STATE_TIME_WAIT) {
            dprintf("tcp: %" PRIu16 ": time wait passed\n", socket->port.num);
        } else {
            dprintf("tcp: %" PRIu16 ": close timeout\n", socket->port.num);
        }

        finish_close(socket, TCP_STATE_CLOSE_COMPLETE, true);
    }

    mutex_unlock(&socket->lock);

    /* Drop the reference added by start_close(). This might free the
     * socket. */
    tcp_socket_release(socket);
    return false;
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
    uint32_t mtu = socket->net.family->mtu;
    // TODO: HACK: We don't implement fragmentation yet so sending larger than
    // device MTU will fail, but we don't have a device yet as we haven't
    // routed. Even once we implement fragmentation, it would be better to get
    // the device MTU to avoid fragmentation. Since we will probably implement
    // caching for routing, we could cache the MTU with the routing information.
    mtu = min(mtu, 1500 - sizeof(ipv4_header_t));
    uint32_t max_segment_size = mtu - sizeof(tcp_header_t);

    bool need_fin = socket->state == TCP_STATE_CLOSE_WAIT || socket->state == TCP_STATE_CLOSE_ACTIVE;
    bool sent_fin = false;

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

        /* If this is the last segment we're going to send for now... */
        if (segment_size == unsent_size) {
            if (need_fin) {
                /* ... send FIN if we're closing. */
                packet.header->flags |= TCP_FIN;
                sent_fin = true;
            } else {
                /* ... else set PSH. */
                packet.header->flags |= TCP_PSH;
            }
        }

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

    if (need_fin) {
        /* We're closing and the buffer was already empty, we'll need to send
         * an empty FIN packet. */
        if (!sent_fin) {
            tcp_tx_packet_t packet;
            ret = prepare_tx_packet(socket, &packet);
            if (ret != STATUS_SUCCESS) {
                kprintf(LOG_WARN, "tcp: failed to route packet: %d\n", ret);
                return;
            }

            packet.header->flags |= TCP_FIN;

            ret = tx_packet(socket, &packet, true);
            if (ret != STATUS_SUCCESS) {
                kprintf(LOG_WARN, "tcp: failed to transmit packet: %d\n", ret);
                return;
            }
        }

        dprintf("tcp: %" PRIu16 ": sent FIN\n", socket->port.num);

        /* FIN increments the sequence number so we expect to have this acked. */
        socket->tx_seq++;

        socket->state = (socket->state == TCP_STATE_CLOSE_WAIT)
            ? TCP_STATE_LAST_ACK
            : TCP_STATE_FIN_WAIT_1;
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
        dprintf("tcp: %" PRIu16 ": received unexpected ACK sequence, ignoring\n", socket->port.num);
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

    mutex_lock(&socket->lock);

    /* If the handle is being closed, there shouldn't be any waiters on it as
     * they'd hold a reference to the handle. */
    assert(list_empty(&socket->state_cvar.threads));

    if (socket->state == TCP_STATE_ESTABLISHED) {
        dprintf("tcp: %" PRIu16 ": initiating active close\n", socket->port.num);

        start_close(socket, TCP_STATE_CLOSE_ACTIVE);

        /* Flush the TX buffer. The FIN will be sent once it is empty which will
         * move to FIN_WAIT_1. */
        flush_tx_buffer(socket);
    }

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

        finish_close(socket, TCP_STATE_CLOSED, false);
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

    if (addr_len > 0)
        return STATUS_ALREADY_CONNECTED;

    if (socket->state != TCP_STATE_ESTABLISHED)
        return (socket->state > TCP_STATE_ESTABLISHED) ? STATUS_SUCCESS : STATUS_NOT_CONNECTED;

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
    // TODO: Support batching up of data to write by not flushing immediately?
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
            return (socket->state > TCP_STATE_ESTABLISHED) ? STATUS_SUCCESS : STATUS_NOT_CONNECTED;

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
    timer_init(&socket->close_timer, "tcp_close_timer", close_timer_func, socket, TIMER_THREAD);

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
            dprintf("tcp: %" PRIu16 ": incorrect sequence number for SYN-ACK, dropping\n", socket->port.num);
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
        dprintf("tcp: %" PRIu16 ": unexpected packet in SYN_SENT state, dropping\n", socket->port.num);
        return;
    }
}

/** Handles packets while in the ESTABLISHED state. */
static void receive_established(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    /* Handle acknowledgement from the sender. */
    ack_tx_buffer(socket, net32_to_cpu(header->ack_num));

    /* data_offset is validated by tcp_receive(). */
    uint32_t data_offset = header->data_offset * sizeof(uint32_t);
    uint32_t data_size   = packet->size - data_offset;

    bool should_ack = data_size > 0 || header->flags & TCP_FIN;

    if (data_size > 0) {
        uint32_t seq_num = net32_to_cpu(header->seq_num);

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
                dprintf(
                    "tcp: %" PRIu16 ": dropping unexpected segment with seq %" PRIu32 " (expecting %" PRIu32 ")\n",
                    socket->port.num, seq_num, socket->rx_seq);
                data_size = 0;
            }
        }

        tcp_buffer_t *buffer = &socket->rx_buffer;

        /* Clamp by what we can fit in the buffer. */
        uint32_t space = buffer->max_size - buffer->curr_size;
        if (data_size > 0 && data_size > space) {
            dprintf(
                "tcp: %" PRIu16 ": RX buffer full, dropping data (received %" PRIu32 " bytes, accepting %" PRIu32 ")\n",
                socket->port.num, data_size, space);
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
    }

    if (header->flags & TCP_FIN)
        socket->rx_seq++;

    /* Acknowledge what we've accepted (if anything). If we received a FIN, this
     * acks it. */
    if (should_ack)
        tx_ack_packet(socket);

    if (header->flags & TCP_FIN) {
        dprintf("tcp: %" PRIu16 ": received FIN, initiating passive close\n", socket->port.num);

        /* CLOSE_WAIT is meant to be waiting until the local client closes the
         * socket, but we immediately initiate the close. We'll move to LAST_ACK
         * once the TX buffer is emptied. */
        start_close(socket, TCP_STATE_CLOSE_WAIT);

        /* Flush the TX buffer. The FIN will be sent once it is empty. */
        flush_tx_buffer(socket);

        /* Wake up receivers, we won't accept any more data. */
        tcp_buffer_t *buffer = &socket->rx_buffer;
        condvar_broadcast(&buffer->cvar);
        notifier_run(&buffer->notifier, NULL, false);
    }
}

/** Handles packets while in the CLOSE_WAIT state. */
static void receive_close_wait(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    ack_tx_buffer(socket, net32_to_cpu(header->ack_num));
}

/** Handles packets while in the LAST_ACK state. */
static void receive_last_ack(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    uint32_t ack_num = net32_to_cpu(header->ack_num);
    if (ack_num >= socket->tx_seq) {
        socket->tx_unack = socket->tx_seq;
        finish_close(socket, TCP_STATE_CLOSE_COMPLETE, false);
    }
}

/** Handles packets while in the FIN_WAIT_1 state. */
static void receive_fin_wait_1(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    /* We're waiting for our FIN to be acked. */
    uint32_t ack_num = net32_to_cpu(header->ack_num);
    if (ack_num >= socket->tx_seq) {
        if (header->flags & TCP_FIN) {
            /* If this has a FIN set we might have missed the acknowledgement
             * or received it out of order, just skip straight to TIME_WAIT. */
            socket->rx_seq++;
            tx_ack_packet(socket);
            socket->state = TCP_STATE_TIME_WAIT;
        } else {
            socket->state = TCP_STATE_FIN_WAIT_2;
        }
    } else if (header->flags & TCP_FIN) {
        /* Simultaneous close. */
        socket->rx_seq++;
        tx_ack_packet(socket);
        socket->state = TCP_STATE_CLOSING;
    }
}

/** Handles packets while in the FIN_WAIT_2 state. */
static void receive_fin_wait_2(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    /* We're waiting for a FIN from the other side. */
    uint32_t ack_num = net32_to_cpu(header->ack_num);
    if (ack_num >= socket->tx_seq && header->flags & TCP_FIN) {
        socket->rx_seq++;
        tx_ack_packet(socket);
        socket->state = TCP_STATE_TIME_WAIT;
    }
}

/** Handles packets while in the CLOSING state. */
static void receive_closing(tcp_socket_t *socket, const tcp_header_t *header, net_packet_t *packet) {
    /* We're waiting for our FIN to be acked. */
    uint32_t ack_num = net32_to_cpu(header->ack_num);
    if (ack_num >= socket->tx_seq) {
        socket->rx_seq++;
        tx_ack_packet(socket);
        socket->state = TCP_STATE_TIME_WAIT;
    }
}

/** Handles a received TCP packet. */
void tcp_receive(net_packet_t *packet, const net_addr_t *source_addr, const net_addr_t *dest_addr) {
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

        dprintf("tcp: %" PRIu16 ": received packet\n", socket->port.num);

        if (socket->state == TCP_STATE_SYN_SENT) {
            receive_syn_sent(socket, header, packet);
        } else if (header->flags & TCP_ACK) {
            switch (socket->state) {
                case TCP_STATE_ESTABLISHED:
                    receive_established(socket, header, packet);
                    break;
                case TCP_STATE_CLOSE_WAIT:
                    receive_close_wait(socket, header, packet);
                    break;
                case TCP_STATE_LAST_ACK:
                    receive_last_ack(socket, header, packet);
                    break;
                case TCP_STATE_FIN_WAIT_1:
                    receive_fin_wait_1(socket, header, packet);
                    break;
                case TCP_STATE_FIN_WAIT_2:
                    receive_fin_wait_2(socket, header, packet);
                    break;
                case TCP_STATE_CLOSING:
                    receive_closing(socket, header, packet);
                    break;
                case TCP_STATE_TIME_WAIT:
                    /* Nothing to do, we're waiting for our timer to expire. */
                    break;
                default:
                    dprintf("tcp: %" PRIu16 ": received packet in unhandled state %d\n", socket->port.num, socket->state);
                    break;
            }
        } else {
            // TODO: Close connection on error?
            dprintf("tcp: %" PRIu16 ": expected ACK to be set but is not, dropping\n", socket->port.num);
        }
    }

    mutex_unlock(&socket->lock);
    tcp_socket_release(socket);
}

static const char *state_string(tcp_state_t state) {
    switch (state) {
        case TCP_STATE_CLOSED:          return "CLOSED";
        case TCP_STATE_REFUSED:         return "REFUSED";
        case TCP_STATE_SYN_SENT:        return "SYN_SENT";
        case TCP_STATE_LISTEN:          return "LISTEN";
        case TCP_STATE_ESTABLISHED:     return "ESTABLISHED";
        case TCP_STATE_CLOSE_ACTIVE:    return "CLOSE_ACTIVE";
        case TCP_STATE_CLOSE_WAIT:      return "CLOSE_WAIT";
        case TCP_STATE_LAST_ACK:        return "LAST_ACK";
        case TCP_STATE_FIN_WAIT_1:      return "FIN_WAIT_1";
        case TCP_STATE_FIN_WAIT_2:      return "FIN_WAIT_2";
        case TCP_STATE_CLOSING:         return "CLOSING";
        case TCP_STATE_TIME_WAIT:       return "TIME_WAIT";
        case TCP_STATE_CLOSE_COMPLETE:  return "CLOSE_COMPLETE";
        default:                        return "???";
    }
}

static kdb_status_t kdb_cmd_tcp4(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Shows details of IPv4 TCP sockets.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("Port   Destination      State\n");
    kdb_printf("====   ===========      =====\n");

    list_foreach(&tcp_ipv4_space.ports, iter) {
        net_port_t *port = list_entry(iter, net_port_t, link);
        tcp_socket_t *socket = container_of(port, tcp_socket_t, port);

        kdb_printf("%-6" PRIu16 " %-16pI4 %s\n",
            socket->port.num, &socket->dest_addr.ipv4.sin_addr, state_string(socket->state));
    }

    return KDB_SUCCESS;
}

static kdb_status_t kdb_cmd_tcp6(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Shows details of IPv6 TCP sockets.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("Port   Destination                              State\n");
    kdb_printf("====   ===========                              =====\n");

    list_foreach(&tcp_ipv6_space.ports, iter) {
        net_port_t *port = list_entry(iter, net_port_t, link);
        tcp_socket_t *socket = container_of(port, tcp_socket_t, port);

        kdb_printf("%-6" PRIu16 " %-40pI6 %s\n",
            socket->port.num, &socket->dest_addr.ipv6.sin6_addr, state_string(socket->state));
    }

    return KDB_SUCCESS;
}

void tcp_init(void) {
    kdb_register_command("tcp4", "Show details of IPv4 TCP sockets.", kdb_cmd_tcp4);
    kdb_register_command("tcp6", "Show details of IPv6 TCP sockets.", kdb_cmd_tcp6);
}
