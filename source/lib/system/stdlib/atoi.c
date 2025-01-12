/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String to integer functions.
 */

#include <stdlib.h>

/** Convert a string to an integer.
 * @param s             String to convert.
 * @return              Converted value. */
int atoi(const char *s) {
    return (int)strtol(s, NULL, 10);
}

/** Convert a string to a double.
 * @param s             String to convert.
 * @return              Converted value. */
double atof(const char *s) {
    return strtod(s, NULL);
}

/** Convert a string to a long long.
 * @param s             String to convert.
 * @return              Converted value. */
long long atoll(const char *s) {
    return strtoll(s, NULL, 10);
}

/** Convert a string to a long.
 * @param s             String to convert.
 * @return              Converted value.
 */
long atol(const char *s) {
    return strtol(s, NULL, 10);
}
