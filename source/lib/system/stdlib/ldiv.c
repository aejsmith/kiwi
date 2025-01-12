/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Integer division function.
 */

#include <stdlib.h>

/** Find the quotient and remainder of an integer division.
 * @param num           Numerator.
 * @param denom         Denominator.
 * @return              Structure containing result of division. */
div_t div(int num, int denom) {
    div_t r;

    r.quot = num / denom;
    r.rem = num % denom;
    if (num >= 0 && r.rem < 0) {
        r.quot++;
        r.rem -= denom;
    } else if (num < 0 && r.rem > 0) {
        r.quot--;
        r.rem += denom;
    }

    return r;
}

/** Find the quotient and remainder of an integer division.
 * @param num           Numerator.
 * @param denom         Denominator.
 * @return              Structure containing result of division. */
ldiv_t ldiv(long num, long denom) {
    ldiv_t r;

    r.quot = num / denom;
    r.rem = num % denom;
    if (num >= 0 && r.rem < 0) {
        r.quot++;
        r.rem -= denom;
    } else if (num < 0 && r.rem > 0) {
        r.quot--;
        r.rem += denom;
    }

    return r;
}
