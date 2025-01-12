/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String length functions.
 */

#include <string.h>

/**
 * Get the length of string.
 *
 * Gets the length of the string specified. The length is the number of
 * characters found before a NULL byte.
 *
 * @param str           Pointer to the string.
 *
 * @return              Length of the string.
 */
size_t strlen(const char *str) {
    size_t ret;

    for (ret = 0; *str; str++, ret++)
        ;

    return ret;
}

/**
 * Get length of string with limit.
 *
 * Gets the length of the string specified. The length is the number of
 * characters found either before a NULL byte or before the maximum length
 * specified.
 *
 * @param str           Pointer to the string.
 * @param count         Maximum length of the string.
 *
 * @return              Length of the string.
 */
size_t strnlen(const char *str, size_t count) {
    size_t ret;

    for (ret = 0; *str && ret < count; str++, ret++)
        ;

    return ret;
}
