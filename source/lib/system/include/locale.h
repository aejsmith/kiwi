/*
 * Copyright (C) 2009-2023 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Locale functions/definitions.
 */

#pragma once

#include <system/defs.h>

#define __NEED_locale_t
#include <bits/alltypes.h>

__SYS_EXTERN_C_BEGIN

/** Locale information structure. */
struct lconv {
    char *currency_symbol;
    char *decimal_point;
    char frac_digits;
    char *grouping;
    char *int_curr_symbol;
    char int_frac_digits;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_p_sign_posn;
    char *mon_decimal_point;
    char *mon_grouping;
    char *mon_thousands_sep;
    char *negative_sign;
    char n_cs_precedes;
    char n_sep_by_space;
    char n_sign_posn;
    char *positive_sign;
    char p_cs_precedes;
    char p_sep_by_space;
    char p_sign_posn;
    char *thousands_sep;
};

/** Categories for setlocale(). */
#define LC_COLLATE          0
#define LC_CTYPE            1
#define LC_MESSAGES         2
#define LC_MONETARY         3
#define LC_NUMERIC          4
#define LC_TIME             5
#define LC_ALL              6

/** Bitmasks for use in the category_mask parameter to newlocale(). */
#define LC_COLLATE_MASK     (1 << LC_COLLATE)
#define LC_CTYPE_MASK       (1 << LC_CTYPE)
#define LC_MESSAGES_MASK    (1 << LC_MESSAGES)
#define LC_MONETARY_MASK    (1 << LC_MONETARY)
#define LC_NUMERIC_MASK     (1 << LC_NUMERIC)
#define LC_TIME_MASK        (1 << LC_TIME)
#define LC_ALL_MASK \
    (LC_COLLATE_MASK | LC_CTYPE_MASK | LC_MESSAGES_MASK | \
        LC_MONETARY_MASK | LC_NUMERIC_MASK | LC_TIME_MASK)

/* locale_t duplocale(locale_t); */
/* void freelocale(locale_t); */
extern struct lconv *localeconv(void);
/* locale_t newlocale(int, const char *, locale_t); */
extern char *setlocale(int category, const char *name);
/* locale_t uselocale(locale_t); */

// Needed for libcxx build.
#ifdef __cplusplus

extern void freelocale(locale_t);
extern locale_t uselocale(locale_t);
extern locale_t newlocale(int, const char *, locale_t);

#endif

__SYS_EXTERN_C_END
