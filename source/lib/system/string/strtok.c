/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String parsing functions.
 */

#include <string.h>

/**
 * Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str           String to parse (NULL to continue last string).
 * @param delim         Set of delimiters.
 * @param saveptr       Where to save state across calls.
 *
 * @return              Pointer to next token, or NULL if no more found.
 */
char *strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {
    char *ret = NULL;

    /* If string is NULL, continue with last operation. */
    if (!str)
        str = *saveptr;

    str += strspn(str, delim);
    if (*str) {
        ret = str;
        str += strcspn(str, delim);
        if (*str)
            *str++ = 0;
    }

    *saveptr = str;
    return ret;
}

/**
 * Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str           String to parse (NULL to continue last string).
 * @param delim         Set of delimiters.
 *
 * @return              Pointer to next token, or NULL if no more found.
 */
char *strtok(char *restrict str, const char *restrict delim) {
    static char *strtok_state = NULL;

    return strtok_r(str, delim, &strtok_state);
}
