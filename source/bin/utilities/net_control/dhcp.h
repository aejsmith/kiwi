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
 * @brief               Network device control utility.
 */

#pragma once

#include <stdint.h>

typedef struct dhcp_header {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic;
    uint8_t options[];
} dhcp_header_t;

#define DHCP_SERVER_PORT            67
#define DHCP_CLIENT_PORT            68

#define DHCP_OP_BOOTREQUEST         1
#define DHCP_OP_BOOTREPLY           2

#define DHCP_MAGIC                  0x63825363

#define DHCP_OPTION_SUBNET_MASK     1
#define DHCP_OPTION_ROUTER          3
#define DHCP_OPTION_MESSAGE_TYPE    53
#define DHCP_OPTION_PARAM_REQUEST   55
#define DHCP_OPTION_END             255

#define DHCP_MESSAGE_DHCPDISCOVER   1
#define DHCP_MESSAGE_DHCPOFFER      2
#define DHCP_MESSAGE_DHCPREQUEST    3
#define DHCP_MESSAGE_DHCPDECLINE    4
#define DHCP_MESSAGE_DHCPACK        5
#define DHCP_MESSAGE_DHCPNAK        6
#define DHCP_MESSAGE_DHCPRELEASE    7
