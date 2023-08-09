/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Network address families.
 */

#include <net/family.h>
#include <net/ipv4.h>

/** Supported network address families. */
static const net_family_t *supported_net_families[__AF_COUNT] = {
    [AF_INET] = &ipv4_net_family,
    //[AF_INET6] TODO
};

/** Gets a network family structure for the given family ID.
 * @param id            Family ID.
 * @return              Operations for the family, or NULL if family is not
 *                      supported. */
const net_family_t *net_family_get(sa_family_t id) {
    return (id < array_size(supported_net_families))
        ? supported_net_families[id]
        : NULL;
}
