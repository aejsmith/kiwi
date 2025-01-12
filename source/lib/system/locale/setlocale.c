/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Set locale function.
 */

#include <locale.h>
#include <string.h>

/**
 * Set the current locale.
 *
 * Sets the current locale for the given category to the locale corresponding
 * to the given string.
 *
 * @param category      Category to set locale for.
 * @param name          Name of locale to set.
 *
 * @return              Name of new locale.
 */
char *setlocale(int category, const char *name) {
    if (name) {
        if (strcmp(name, "C") && strcmp(name, "POSIX") && strcmp(name, ""))
            return NULL;
    }

    return (char *)"C";
}
