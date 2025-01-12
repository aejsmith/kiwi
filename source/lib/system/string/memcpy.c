/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory copying function.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 *
 * @return              Destination location.
 */
void *memcpy(void *__restrict dest, const void *__restrict src, size_t count) {
    const char *s = (const char *)src;
    const unsigned long *ns;
    char *d = (char *)dest;
    unsigned long *nd;

    /* Align the destination. */
    while ((uintptr_t)d & (sizeof(unsigned long) - 1)) {
        if (count--) {
            *d++ = *s++;
        } else {
            return dest;
        }
    }

    /* Write in native-sized blocks if we can. */
    if (count >= sizeof(unsigned long)) {
        nd = (unsigned long *)d;
        ns = (const unsigned long *)s;

        /* Unroll the loop if possible. */
        while (count >= (sizeof(unsigned long) * 4)) {
            *nd++ = *ns++;
            *nd++ = *ns++;
            *nd++ = *ns++;
            *nd++ = *ns++;
            count -= sizeof(unsigned long) * 4;
        }
        while (count >= sizeof(unsigned long)) {
            *nd++ = *ns++;
            count -= sizeof(unsigned long);
        }

        d = (char *)nd;
        s = (const char *)ns;
    }

    /* Write remaining bytes. */
    while (count--)
        *d++ = *s++;

    return dest;
}
