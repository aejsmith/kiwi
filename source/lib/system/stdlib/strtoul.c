/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String to integer functions.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/** Macro to implement strtoul() and strtoull(). */
#define __strtoux(type, max, cp, endp, base) \
    __extension__ \
    ({ \
        type result = 0, value; \
        if (!base) { \
            if (*cp == '0') { \
                if ((tolower(*(++cp)) == 'x') && isxdigit(cp[1])) { \
                    cp++; \
                    base = 16; \
                } else { \
                    base = 8; \
                } \
            } else { \
                base = 10; \
            } \
        } else if (base == 8) { \
            if (cp[0] == '0') { \
                cp++; \
            } \
        } else if (base == 16) { \
            if (cp[0] == '0' && tolower(cp[1]) == 'x') { \
                cp += 2; \
            } \
        } else if (base != 10) { \
            errno = EINVAL; \
            return max; \
        } \
        while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : tolower(*cp) - 'a' + 10) < (unsigned int)base) { \
            result = result * (unsigned int)base + value; \
            cp++; \
        } \
        if (endp) { \
            *endp = (char *)cp; \
        } \
        result; \
    })

/** Convert a string to an unsigned long.
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use (if zero will guess).
 * @return              Converted value. */
unsigned long strtoul(const char *restrict cp, char **restrict endp, int base) {
    return __strtoux(unsigned long, ULONG_MAX, cp, endp, base);
}

/** Convert a string to a signed long.
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 * @return              Converted value. */
long strtol(const char *restrict cp, char **restrict endp, int base) {
    if (*cp == '-')
        return -strtoul(cp + 1, endp, base);

    return strtoul(cp, endp, base);
}

/** Convert a string to an unsigned long long.
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 * @return              Converted value. */
unsigned long long int strtoull(const char *restrict cp, char **restrict endp, int base) {
    return __strtoux(unsigned long long int, ULLONG_MAX, cp, endp, base);
}

uintmax_t strtoumax(const char *__restrict cp, char **__restrict endp, int base) {
    return strtoull(cp, endp, base);
}

/** Convert a string to a signed long long.
 * @param cp            The start of the string.
 * @param endp          Pointer to the end of the parsed string placed here.
 * @param base          The number base to use.
 * @return              Converted value. */
long long int strtoll(const char *restrict cp, char **restrict endp, int base) {
    if (*cp == '-')
        return -strtoull(cp + 1, endp, base);

    return strtoull(cp, endp, base);
}

intmax_t strtoimax(const char *__restrict cp, char **__restrict endp, int base) {
    return strtoll(cp, endp, base);
}
