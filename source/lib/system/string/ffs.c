/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Find first set bit function.
 */

#include <strings.h>

/** Find the first bit set in a value.
 * @param i             Value to search in.
 * @return              Position of first set bit + 1, or 0 if none set. */
int ffs(int i) {
    return __builtin_ffs(i);
}
