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

#pragma once

#include <kernel/net/ipv4.h>

#include <net/interface.h>
#include <net/socket.h>

struct net_packet;

/** IPv4 header structure. */
typedef struct ipv4_header {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t ihl     : 4;
    uint8_t version : 4;
#else
    uint8_t version : 4;
    uint8_t ihl     : 4;
#endif
    uint8_t dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_offset_flags;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t source_addr;
    uint32_t dest_addr;
} __packed ipv4_header_t;

/** Maximum IPv4 packet size and MTU (payload size). */
#define IPV4_MAX_PACKET_SIZE    65535
#define IPV4_MTU                65515

extern const net_addr_ops_t ipv4_net_addr_ops;

extern status_t ipv4_socket_create(sa_family_t family, int type, int protocol, socket_t **_socket);

extern void ipv4_receive(net_interface_t *interface, struct net_packet *packet);
