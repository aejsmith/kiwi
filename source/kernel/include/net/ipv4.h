/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    uint16_t total_size;
    uint16_t id;
    uint16_t frag_offset_flags;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t source_addr;
    uint32_t dest_addr;
} __packed ipv4_header_t;

/** Pseudo IPv4 header used by TCP and UDP checksums. */
typedef struct ipv4_pseudo_header {
    uint32_t source_addr;
    uint32_t dest_addr;
    uint8_t zero;
    uint8_t protocol;
    uint16_t length;
} __packed ipv4_pseudo_header_t;

#define IPV4_HEADER_FRAG_OFFSET_MASK    0x1fff
#define IPV4_HEADER_FRAG_FLAGS_MF       0x2000

/** Maximum IPv4 packet size and MTU (payload size). */
#define IPV4_MAX_PACKET_SIZE            65535
#define IPV4_MTU                        65515

extern const net_family_t ipv4_net_family;

extern status_t ipv4_socket_create(sa_family_t family, int type, int protocol, socket_t **_socket);

extern void ipv4_receive(net_interface_t *interface, struct net_packet *packet);

extern void ipv4_init(void);
