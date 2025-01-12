/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory setting function.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** Fill a memory area.
 * @param dest          The memory area to fill.
 * @param val           The value to fill with (converted to an unsigned char).
 * @param count         The number of bytes to fill.
 * @return              Destination location. */
void *memset(void *dest, int val, size_t count) {
    unsigned char c = val & 0xff;
    unsigned long *nd, nval;
    char *d = (char *)dest;

    /* Align the destination. */
    while ((uintptr_t)d & (sizeof(unsigned long) - 1)) {
        if (count--) {
            *d++ = c;
        } else {
            return dest;
        }
    }

    /* Write in native-sized blocks if we can. */
    if (count >= sizeof(unsigned long)) {
        nd = (unsigned long *)d;

        /* Compute the value we will write. */
        #if __WORDSIZE == 64
            nval = c * 0x0101010101010101ul;
        #elif __WORDSIZE == 32
            nval = c * 0x01010101ul;
        #else
        #   error "Unsupported"
        #endif

        /* Unroll the loop if possible. */
        while (count >= (sizeof(unsigned long) * 4)) {
            *nd++ = nval;
            *nd++ = nval;
            *nd++ = nval;
            *nd++ = nval;
            count -= sizeof(unsigned long) * 4;
        }
        while (count >= sizeof(unsigned long)) {
            *nd++ = nval;
            count -= sizeof(unsigned long);
        }

        d = (char *)nd;
    }

    /* Write remaining bytes. */
    while (count--)
        *d++ = val;

    return dest;
}
