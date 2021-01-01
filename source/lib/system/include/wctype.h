/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Wide-character classification and mapping utilities.
 */

#pragma once

#include <locale.h>
#include <wchar.h>

__SYS_EXTERN_C_BEGIN

/** Type that can hold locale-specific character mappings. */
typedef const int32_t *wctrans_t;

/* int iswalnum(wint_t); */
/* int iswalnum_l(wint_t, locale_t); */
/* int iswalpha(wint_t); */
/* int iswalpha_l(wint_t, locale_t); */
/* int iswblank(wint_t); */
/* int iswblank_l(wint_t, locale_t); */
/* int iswcntrl(wint_t); */
/* int iswcntrl_l(wint_t, locale_t); */
/* int iswctype(wint_t, wctype_t); */
/* int iswctype_l(wint_t, wctype_t, locale_t); */
/* int iswdigit(wint_t); */
/* int iswdigit_l(wint_t, locale_t); */
/* int iswgraph(wint_t); */
/* int iswgraph_l(wint_t, locale_t); */
/* int iswlower(wint_t); */
/* int iswlower_l(wint_t, locale_t); */
/* int iswprint(wint_t); */
/* int iswprint_l(wint_t, locale_t); */
/* int iswpunct(wint_t); */
/* int iswpunct_l(wint_t, locale_t); */
/* int iswspace(wint_t); */
/* int iswspace_l(wint_t, locale_t); */
/* int iswupper(wint_t); */
/* int iswupper_l(wint_t, locale_t); */
/* int iswxdigit(wint_t); */
/* int iswxdigit_l(wint_t, locale_t); */
/* wint_t towctrans(wint_t, wctrans_t); */
/* wint_t towctrans_l(wint_t, wctrans_t, locale_t); */
/* wint_t towlower(wint_t); */
/* wint_t towlower_l(wint_t, locale_t); */
/* wint_t towupper(wint_t); */
/* wint_t towupper_l(wint_t, locale_t); */
/* wctrans_t wctrans(const char *); */
/* wctrans_t wctrans_l(const char *, locale_t); */
/* wctype_t wctype(const char *); */
/* wctype_t wctype_l(const char *, locale_t); */

// Needed for libcxx build.
#ifdef __cplusplus

extern int iswalnum(wint_t);
extern int iswalnum_l(wint_t, locale_t);
extern int iswalpha(wint_t);
extern int iswalpha_l(wint_t, locale_t);
extern int iswblank(wint_t);
extern int iswblank_l(wint_t, locale_t);
extern int iswcntrl(wint_t);
extern int iswcntrl_l(wint_t, locale_t);
extern int iswctype(wint_t, wctype_t);
extern int iswctype_l(wint_t, wctype_t, locale_t);
extern int iswdigit(wint_t);
extern int iswdigit_l(wint_t, locale_t);
extern int iswgraph(wint_t);
extern int iswgraph_l(wint_t, locale_t);
extern int iswlower(wint_t);
extern int iswlower_l(wint_t, locale_t);
extern int iswprint(wint_t);
extern int iswprint_l(wint_t, locale_t);
extern int iswpunct(wint_t);
extern int iswpunct_l(wint_t, locale_t);
extern int iswspace(wint_t);
extern int iswspace_l(wint_t, locale_t);
extern int iswupper(wint_t);
extern int iswupper_l(wint_t, locale_t);
extern int iswxdigit(wint_t);
extern int iswxdigit_l(wint_t, locale_t);
extern wint_t towctrans(wint_t, wctrans_t);
extern wint_t towctrans_l(wint_t, wctrans_t, locale_t);
extern wint_t towlower(wint_t);
extern wint_t towlower_l(wint_t, locale_t);
extern wint_t towupper(wint_t);
extern wint_t towupper_l(wint_t, locale_t);
extern wctrans_t wctrans(const char *);
extern wctrans_t wctrans_l(const char *, locale_t);
extern wctype_t wctype(const char *);
extern wctype_t wctype_l(const char *, locale_t);

#endif

__SYS_EXTERN_C_END
