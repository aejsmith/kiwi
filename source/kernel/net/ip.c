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
 * @brief               IPv4/6 common definitions.
 */

#include <net/ip.h>
#include <net/ipv4.h>
#include <net/ipv6.h>
#include <net/net.h>

#include <kernel.h>

static uint32_t add_checksum(const void *data, size_t size, uint32_t sum) {
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i += 2) {
        uint16_t val = (uint16_t)bytes[i] << 8;
        val |= (i < size - 1) ? bytes[i + 1] : 0;

        sum += val;
        if (sum > 0xffff)
            sum = (sum >> 16) + (sum & 0xffff);
    }

    return sum;
}

/** Calculates an IP checksum for a given set of data.
 * @param data          Data to checksum.
 * @param size          Size of data.
 * @return              Calculated checksum. */
uint16_t ip_checksum(const void *data, size_t size) {
    uint32_t sum = add_checksum(data, size, 0);
    return cpu_to_net16((uint16_t)(~sum));
}

/**
 * Calculates an IP checksum for a given set of data with a pseudo-header
 * attached.
 *
 * @param data          Data to checksum.
 * @param size          Size of data.
 * @param protocol      IP protocol number.
 * @param source_addr   Source address.
 * @param dest_addr     Destination address.
 *
 * @return              Calculated checksum.
 */
uint16_t ip_checksum_pseudo(
    const void *data, size_t size, uint8_t protocol,
    const sockaddr_ip_t *source_addr, const sockaddr_ip_t *dest_addr)
{
    uint32_t sum = 0;

    if (source_addr->family == AF_INET6) {
        kprintf(LOG_ERROR, "ip: TODO: IPv6 checksum\n");
    } else {
        ipv4_pseudo_header_t header;
        header.source_addr = source_addr->ipv4.sin_addr.val;
        header.dest_addr   = dest_addr->ipv4.sin_addr.val;
        header.zero        = 0;
        header.protocol    = protocol;
        header.length      = cpu_to_net16(size);

        sum = add_checksum(&header, sizeof(header), sum);
    }

    sum = add_checksum(data, size, sum);

    return cpu_to_net16((uint16_t)(~sum));
}
