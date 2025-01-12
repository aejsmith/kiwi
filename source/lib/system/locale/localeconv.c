/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
