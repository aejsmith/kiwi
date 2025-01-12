/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               UDP protocol implementation.
 */

#pragma once

#include <net/ip.h>
#include <net/socket.h>

struct net_packet;

/** UDP packet header. */
typedef struct udp_header {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __packed udp_header_t;

/** Maximum UDP packet size (without IP header). */
#define UDP_MAX_PACKET_SIZE     65535

extern status_t udp_socket_create(sa_family_t family, socket_t **_socket);

extern void udp_receive(struct net_packet *packet, const net_addr_t *source_addr, const net_addr_t *dest_addr);
