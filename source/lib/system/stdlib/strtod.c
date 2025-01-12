/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String to double function.
 */

#include "libsystem.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/** Convert a string to a double precision number.
 * @param s             String to convert.
 * @param endptr        Pointer to store end of string in (can be NULL).
 * @return              Converted number. */
double strtod(const char *restrict s, char **restrict endptr) {
    const char *p = s;
    long double factor, value = 0.L;
    int sign = 1;
    unsigned int expo = 0;

    while (isspace(*p))
        p++;

    switch (*p) {
    case '-':
        sign = -1;
    case '+':
        p++;
    default:
        break;
    }

    while ((unsigned int)(*p - '0') < 10u)
        value = value * 10 + (*p++ - '0');

    if (*p == '.') {
        factor = 1.;
        p++;

        while ((unsigned int)(*p - '0') < 10u) {
            factor *= 0.1;
            value += (*p++ - '0') * factor;
        }
    }

    if ((*p | 32) == 'e') {
        factor = 10.L;

        switch (*++p) {
        case '-':
            factor = 0.1;
        case '+':
            p++;
            break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            break;
        default:
            value = 0.L;
            p = s;
            goto done;
        }

        while ((unsigned int)(*p - '0') < 10u)
            expo = 10 * expo + (*p++ - '0');

        while (1) {
            if (expo & 1)
                value *= factor;
            if ((expo >>= 1) == 0)
                break;
            factor *= factor;
        }
    }

done:
    if (endptr)
        *endptr = (char *)p;

    return value * sign;
}

/** Convert a string to a double precision number.
 * @param s             String to convert.
 * @param endptr        Pointer to store end of string in (can be NULL).
 * @return              Converted number. */
long double strtold(const char *__restrict s, char **__restrict endptr) {
    libsystem_stub("strtold", false);

    if (endptr)
        *endptr = (char *)s;

    return 0;
}
