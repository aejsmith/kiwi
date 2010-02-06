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
