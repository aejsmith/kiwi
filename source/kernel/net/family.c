/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
