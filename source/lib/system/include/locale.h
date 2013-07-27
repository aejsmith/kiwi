/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Locale functions/definitions.
 */

#ifndef __LOCALE_H
#define __LOCALE_H

#ifdef __cplusplus
extern "C" {
#endif

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
#define LC_ALL		0
#define LC_COLLATE	1
#define LC_CTYPE	2
#define LC_MESSAGES	3
#define LC_MONETARY	4
#define LC_NUMERIC	5
#define LC_TIME		6

extern struct lconv *localeconv(void);
extern char *setlocale(int category, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* __LOCALE_H */
