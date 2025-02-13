/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IPv4/6 common definitions.
 */

#include <net/ip.h>
#include <net/ipv4.h>
#include <net/ipv6.h>
#include <net/net.h>
#include <net/packet.h>

#include <assert.h>
#include <kernel.h>

/**
 * Check if a socket address matches a packet's address/port. Useful for
 * comparing received address/port to a socket's bound address.
 *
 * @param a             Socket address.
 * @param b_addr        Address to compare with.
 * @param b_port        Port to compare with (in network byte order).
 *
 * @return              Whether the addresses match.
 */
bool ip_sockaddr_equal(const sockaddr_ip_t *a, const net_addr_t *b_addr, uint16_t b_port) {
    bool ret = false;

    if (a->family == b_addr->family) {
        ret = a->port == b_port;

        if (a->family == AF_INET) {
            ret &= a->ipv4.sin_addr.val == b_addr->ipv4.val;
        } else if (a->family == AF_INET6) {
            ret &= memcmp(a->ipv6.sin6_addr.bytes, b_addr->ipv6.bytes, sizeof(b_addr->ipv6.bytes)) == 0;
        }
    }

    return ret;
}

static inline uint32_t add_bytes(uint8_t first, uint8_t second, uint32_t sum) {
    uint16_t val = ((uint16_t)first << 8) | second;
    sum += val;
    if (sum > 0xffff)
        sum = (sum >> 16) + (sum & 0xffff);
    return sum;
}

static inline uint32_t add_checksum(const void *data, size_t size, uint32_t sum) {
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < size; i += 2) {
        sum = add_bytes(
            bytes[i],
            (i < size - 1) ? bytes[i + 1] : 0,
            sum);
    }

    return sum;
}

static uint32_t add_pseudo_checksum(
    size_t size, uint8_t protocol, const net_addr_t *source_addr,
    const net_addr_t *dest_addr, uint32_t sum)
{
    if (source_addr->family == AF_INET6) {
        kprintf(LOG_ERROR, "ip: TODO: IPv6 checksum\n");
    } else {
        ipv4_pseudo_header_t header;
        header.source_addr = source_addr->ipv4.val;
        header.dest_addr   = dest_addr->ipv4.val;
        header.zero        = 0;
        header.protocol    = protocol;
        header.length      = cpu_to_net16(size);

        sum = add_checksum(&header, sizeof(header), sum);
    }

    return sum;
}

static uint32_t add_packet_checksum(net_packet_t *packet, uint32_t offset, uint32_t size, uint32_t sum) {
    assert(size > 0);
    assert(offset < packet->size);
    assert(offset + size <= packet->size);

    net_buffer_t *buffer = packet->head;
    bool carry = false;
    uint8_t carry_val = 0;

    while (size > 0) {
        assert(buffer);

        uint32_t remaining = buffer->size - buffer->offset;

        if (offset < remaining) {
            uint32_t buf_size = min(size, remaining - offset);

            if (buffer->type == NET_BUFFER_TYPE_REF) {
                // TODO: This is awkward for handling non-2-byte-aligned buffers.
                kprintf(LOG_ERROR, "ip: TODO: checksum doesn't handle REF buffers\n");
                return 0;
            } else {
                uint32_t sum_size = buf_size;
                uint8_t *sum_data = net_buffer_data(buffer, offset, buf_size);
                assert(sum_data);

                /* Checksum is calculated in 2 byte words, we need to handle
                 * non-2-byte-aligned buffers. */
                if (carry) {
                    carry = false;
                    sum   = add_bytes(carry_val, sum_data[0], sum);
                    sum_size--;
                    sum_data++;
                }

                sum = add_checksum(sum_data, sum_size & ~1, sum);

                if (sum_size & 1) {
                    carry     = true;
                    carry_val = sum_data[sum_size - 1];
                }
            }

            size  -= buf_size;
            offset = 0;
        } else {
            offset -= remaining;
        }

        buffer = buffer->next;
    }

    if (carry) {
        /* Include last byte with 0 padding. */
        sum = add_bytes(carry_val, 0, sum);
    }

    return sum;
}

static uint16_t finish_checksum(uint32_t sum) {
    return cpu_to_net16((uint16_t)(~sum));
}

/** Calculates an IP checksum for a given set of data.
 * @param data          Data to checksum.
 * @param size          Size of data.
 * @return              Calculated checksum. */
uint16_t ip_checksum(const void *data, size_t size) {
    uint32_t sum = add_checksum(data, size, 0);
    return finish_checksum(sum);
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
    const net_addr_t *source_addr, const net_addr_t *dest_addr)
{
    uint32_t sum = 0;

    sum = add_pseudo_checksum(size, protocol, source_addr, dest_addr, sum);
    sum = add_checksum(data, size, sum);

    return finish_checksum(sum);
}

/**
 * Calculates an IP checksum for a subset of packet data with a pseudo-header
 * attached.
 *
 * @param packet        Packet containing data to checksum.
 * @param offset        Offset of data.
 * @param size          Size of data.
 * @param protocol      IP protocol number.
 * @param source_addr   Source address.
 * @param dest_addr     Destination address.
 *
 * @return              Calculated checksum.
 */
uint16_t ip_checksum_packet_pseudo(
    net_packet_t *packet, uint32_t offset, uint32_t size, uint8_t protocol,
    const net_addr_t *source_addr, const net_addr_t *dest_addr)
{
    uint32_t sum = 0;

    sum = add_pseudo_checksum(size, protocol, source_addr, dest_addr, sum);
    sum = add_packet_checksum(packet, offset, size, sum);

    return finish_checksum(sum);
}
