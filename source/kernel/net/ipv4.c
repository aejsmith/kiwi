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
    .size  = sizeof(net_addr_ipv4_t),

    .valid = ipv4_net_addr_valid,
    .equal = ipv4_net_addr_equal,
};

/** Creates an IPv4 socket. */
status_t ipv4_socket_create(sa_family_t family, int type, int protocol, socket_t **_socket) {
    switch (type) {
        case SOCK_DGRAM: {
            switch (protocol) {
                case IPPROTO_IP:
                case IPPROTO_UDP:
                    return udp_socket_create(family, _socket);
            }

            break;
        }
        //case SOCK_STREAM: {
        //  break;
        //}
    }

    return STATUS_INVALID_ARG;
}
