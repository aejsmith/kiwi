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
 * @brief               Address Resolution Protocol.
 */

#include <net/arp.h>

status_t arp_lookup(
    net_interface_t *interface, const sockaddr_in_t *source_addr,
    const sockaddr_in_t *dest_addr, uint8_t *_dest_hw_addr)
{
    // TODO: Implement properly once receive path is implemented. This is for
    // QEMU user networking.
    _dest_hw_addr[0] = 0x52;
    _dest_hw_addr[1] = 0x55;
    _dest_hw_addr[2] = 0x0a;
    _dest_hw_addr[3] = 0x00;
    _dest_hw_addr[4] = 0x02;
    _dest_hw_addr[5] = 0x02;
    return STATUS_SUCCESS;
}
