/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Locale information function.
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

/** Get locale information.
 *
 * Gets locale-specific information for the current locale.
 *
 * @return		Pointer to locale information structure.
 */
struct lconv *localeconv(void) {
	return (struct lconv *)&__libsystem_locale;
}
