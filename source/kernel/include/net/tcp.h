/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               TCP protocol implementation.
 */

#pragma once

#include <net/ip.h>
#include <net/socket.h>

struct net_packet;

/** TCP packet header. */
typedef struct tcp_header {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t reserved : 4;
    uint8_t data_offset : 4;
#else
    uint8_t data_offset : 4;
    uint8_t reserved : 4;
#endif
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urg_ptr;
} __packed tcp_header_t;

#define TCP_FIN             (1<<0)
#define TCP_SYN             (1<<1)
#define TCP_RST             (1<<2)
#define TCP_PSH             (1<<3)
#define TCP_ACK             (1<<4)
#define TCP_URG             (1<<5)
#define TCP_ECE             (1<<6)
#define TCP_CWR             (1<<7)

#define TCP_SEQ_LT(a, b)    (((uint32_t)((uint32_t)(a) - (uint32_t)(b)) & (1u<<31)) != 0)
#define TCP_SEQ_LE(a, b)    (!TCP_SEQ_LT(b, a))
#define TCP_SEQ_GT(a, b)    TCP_SEQ_LT(b, a)
#define TCP_SEQ_GE(a, b)    (!TCP_SEQ_LT(a, b))

extern status_t tcp_socket_create(sa_family_t family, socket_t **_socket);

extern void tcp_receive(struct net_packet *packet, const net_addr_t *source_addr, const net_addr_t *dest_addr);

extern void tcp_init(void);
