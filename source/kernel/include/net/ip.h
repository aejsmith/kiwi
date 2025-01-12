/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IPv4/6 common definitions.
 */

#pragma once

#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/socket.h>

#include <net/family.h>

struct net_packet;

/** Socket address union supporting both IPv4 and IPv6. */
typedef union sockaddr_ip {
    struct {
        uint16_t family;
        uint16_t port;
    };
    sockaddr_in_t ipv4;
    sockaddr_in6_t ipv6;
} sockaddr_ip_t;

/** Default range to use for ephemeral (dynamic) ports (IANA standard range). */
#define IP_EPHEMERAL_PORT_FIRST    49152
#define IP_EPHEMERAL_PORT_LAST     65535

extern bool ip_sockaddr_equal(const sockaddr_ip_t *a, const net_addr_t *b_addr, uint16_t b_port);

extern uint16_t ip_checksum(const void *data, size_t size);

extern uint16_t ip_checksum_pseudo(
    const void *data, size_t size, uint8_t protocol,
    const net_addr_t *source_addr, const net_addr_t *dest_addr);
extern uint16_t ip_checksum_packet_pseudo(
    struct net_packet *packet, uint32_t offset, uint32_t size, uint8_t protocol,
    const net_addr_t *source_addr, const net_addr_t *dest_addr);
