/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Absolute value functions.
 */

#include <stdlib.h>

/** Compute the absolute value of an integer.
 * @param j             Integer to compute from. */
int abs(int j) {
    return (j < 0) ? -j : j;
}

/** Compute the absolute value of a long integer.
 * @param j             Long integer to compute from. */
long labs(long j) {
    return (j < 0) ? -j : j;
}

/** Compute the absolute value of a long long integer.
 * @param j             Long long integer to compute from. */
long long llabs(long long j) {
    return (j < 0) ? -j : j;
}
