/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String separating function.
 */

#include <string.h>

/**
 * Separate a string.
 *
 * Finds the first occurrence of a symbol in the string delim in *stringp.
 * If one is found, the delimeter is replaced by a NULL byte and the pointer
 * pointed to by stringp is updated to point past the string. If no delimeter
 * is found *stringp is made NULL and the token is taken to be the entire
 * string.
 *
 * @param stringp       Pointer to a pointer to the string to separate.
 * @param delim         String containing all possible delimeters.
 *
 * @return              NULL if stringp is NULL, otherwise a pointer to the
 *                      token found.
 */
char *strsep(char **stringp, const char *delim) {
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    s = *stringp;
    if (!s)
        return NULL;

    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            sc = *spanp++;
            if (sc == c) {
                if (c == 0) {
                    s = NULL;
                } else {
                    s[-1] = 0;
                }

                *stringp = s;
                return tok;
            }
        } while (sc != 0);
    }
}
