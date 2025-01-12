/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Ethernet link layer support.
 */

#pragma once

#include <net/interface.h>

/** Ethernet MAC address length. */
#define ETHERNET_ADDR_LEN           6

/** Ethernet frame size definitions. */
#define ETHERNET_HEADER_SIZE        14      /**< Ethernet header size. */
#define ETHERNET_MIN_PAYLOAD_SIZE   46      /**< Minimum payload size. */
#define ETHERNET_MAX_PAYLOAD_SIZE   1500    /**< Minimum payload size. */
#define ETHERNET_MTU                ETHERNET_MAX_PAYLOAD_SIZE
#define ETHERNET_MIN_FRAME_SIZE     60      /**< Minimum frame (header + payload) size, minus FCS. */
#define ETHERNET_MAX_FRAME_SIZE     1514    /**< Maximum frame size, minus FCS. */

/** Ethernet frame header. */
typedef struct ethernet_header {
    uint8_t dest_addr[ETHERNET_ADDR_LEN];
    uint8_t source_addr[ETHERNET_ADDR_LEN];
    uint16_t type;
} __packed ethernet_header_t;

extern const net_link_ops_t ethernet_net_link_ops;
