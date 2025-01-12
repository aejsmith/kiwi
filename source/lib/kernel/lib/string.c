/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel library support functions.
 */

#include <stdlib.h>
#include <string.h>

/** Get the length of a string.
 * @param str           Pointer to the string.
 * @return              Length of the string. */
size_t strlen(const char *str) {
    size_t ret;

    for (ret = 0; *str; str++, ret++)
        ;

    return ret;
}

/** Fill a memory area.
 * @param dest          The memory area to fill.
 * @param val           The value to fill with.
 * @param count         The number of bytes to fill.
 * @return              Destination address. */
void *memset(void *dest, int val, size_t count) {
    char *temp = (char *)dest;

    for (; count != 0; count--)
        *temp++ = val;

    return dest;
}

/** Copy data in memory.
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 * @return              Destination address. */
void *memcpy(void *dest, const void *src, size_t count) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    size_t i;

    for (i = 0; i < count; i++)
        *d++ = *s++;

    return dest;
}

/** Find first occurrence of a character in a string.
 * @param s             Pointer to the string to search.
 * @param c             Character to search for.
 * @return              NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
    char ch = c;

    while (true) {
        if (*s == ch) {
            break;
        } else if (!*s) {
            return NULL;
        } else {
            s++;
        }
    }

    return (char *)s;
}

/** Copy a string.
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 * @return              The value specified for dest. */
char *strcpy(char *__restrict dest, const char *__restrict src) {
    char *d = dest;

    while ((*d++ = *src++))
        ;

    return dest;
}

/** Copy a string with a length limit.
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 * @param count         Maximum number of bytes to copy.
 * @return              The value specified for dest. */
char *strncpy(char *__restrict dest, const char *__restrict src, size_t count) {
    size_t i;

    for (i = 0; i < count; i++) {
        dest[i] = src[i];
        if (!src[i])
            break;
    }

    return dest;
}

/** Concatenate two strings.
 * @param dest          Pointer to the string to append to.
 * @param src           Pointer to the string to append.
 * @return              Pointer to dest. */
char *strcat(char *__restrict dest, const char *__restrict src) {
    size_t len = strlen(dest);
    char *d = dest + len;

    while ((*d++ = *src++))
        ;

    return dest;
}

/** Compare two strings.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    while (true) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;
    }
}

/** Compare two strings with a length limit.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @param count         Maximum number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
    unsigned char c1, c2;

    while (count) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;

        count--;
    }

    return 0;
}

/** Duplicate a string.
 * @param s         Pointer to the source buffer.
 * @return          Pointer to the allocated buffer containing the string,
 *                  or NULL on failure. */
char *strdup(const char *s) {
    char *dup;
    size_t len = strlen(s) + 1;

    dup = malloc(len);
    if (!dup)
        return NULL;

    memcpy(dup, s, len);
    return dup;
}

/** Separate a string.
 * @param stringp       Pointer to a pointer to the string to separate.
 * @param delim         String containing all possible delimeters.
 * @return              NULL if stringp is NULL, otherwise a pointer to the
 *                      token found. */
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
