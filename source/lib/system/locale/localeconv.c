/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Locale information function.
 */

#include <limits.h>
#include <locale.h>

/** Structure defining the C locale. */
static const struct lconv __libsystem_locale = {
    .currency_symbol = (char *)"",
    .decimal_point = (char *)".",
    .frac_digits = CHAR_MAX,
    .grouping = (char *)"",
    .int_curr_symbol = (char *)"",
    .int_frac_digits = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .mon_decimal_point = (char *)"",
    .mon_grouping = (char *)"",
    .mon_thousands_sep = (char *)"",
    .negative_sign = (char *)"",
    .n_cs_precedes = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .positive_sign = (char *)"",
    .p_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .thousands_sep = (char *)"",
};

/** Get information about the current locale.
 * @return              Pointer to locale information structure. */
struct lconv *localeconv(void) {
    return (struct lconv *)&__libsystem_locale;
}
