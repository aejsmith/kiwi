/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Wide-character handling.
 */

#pragma once

#include <locale.h>
#include <stdarg.h>
#define __need_wint_t
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tm;

/** Value used to indicate EOF. */
#define WEOF        (0xffffffffu)

/** Structure holding multi-byte conversion state. */
typedef struct {
    int dummy;
} mbstate_t;

/** Scalar type that can hold locale-specific character classifications. */
typedef unsigned long wctype_t;

/* wint_t btowc(int); */
/* wint_t fgetwc(FILE *); */
/* wchar_t *fgetws(wchar_t *__restrict, int, FILE *__restrict); */
/* wint_t fputwc(wchar_t, FILE *); */
/* int fputws(const wchar_t *__restrict, FILE *__restrict); */
/* int fwide(FILE *, int); */
/* int fwprintf(FILE *__restrict, const wchar_t *__restrict, ...); */
/* int fwscanf(FILE *__restrict, const wchar_t *__restrict, ...); */
/* wint_t getwc(FILE *); */
/* wint_t getwchar(void); */
/* size_t mbrlen(const char *__restrict, size_t, mbstate_t *__restrict); */
/* size_t mbrtowc(wchar_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict); */
/* int mbsinit(const mbstate_t *); */
/* size_t mbsnrtowcs(wchar_t *__restrict, const char **__restrict, size_t, size_t, mbstate_t *__restrict); */
/* size_t mbsrtowcs(wchar_t *__restrict, const char **__restrict, size_t, mbstate_t *__restrict); */
/* FILE  *open_wmemstream(wchar_t **, size_t *); */
/* wint_t putwc(wchar_t, FILE *); */
/* wint_t putwchar(wchar_t); */
/* int swprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, ...); */
/* int swscanf(const wchar_t *__restrict, const wchar_t *__restrict, ...); */
/* wint_t ungetwc(wint_t, FILE *); */
/* int vfwprintf(FILE *__restrict, const wchar_t *__restrict, va_list); */
/* int vfwscanf(FILE *__restrict, const wchar_t *__restrict, va_list); */
/* int vswprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, va_list); */
/* int vswscanf(const wchar_t *__restrict, const wchar_t *__restrict, va_list); */
/* int vwprintf(const wchar_t *__restrict, va_list); */
/* int vwscanf(const wchar_t *__restrict, va_list); */
/* wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict); */
/* wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t); */
/* size_t wcrtomb(char *__restrict, wchar_t, mbstate_t *__restrict); */
/* int wcscasecmp(const wchar_t *, const wchar_t *); */
/* int wcscasecmp_l(const wchar_t *, const wchar_t *, locale_t); */
/* wchar_t *wcscat(wchar_t *__restrict, const wchar_t *__restrict); */
/* wchar_t *wcschr(const wchar_t *, wchar_t); */
/* int wcscmp(const wchar_t *, const wchar_t *); */
/* int wcscoll(const wchar_t *, const wchar_t *); */
/* int wcscoll_l(const wchar_t *, const wchar_t *, locale_t); */
/* wchar_t *wcscpy(wchar_t *__restrict, const wchar_t *__restrict); */
/* size_t wcscspn(const wchar_t *, const wchar_t *); */
/* wchar_t *wcsdup(const wchar_t *); */
/* size_t wcsftime(wchar_t *__restrict, size_t, const wchar_t *__restrict, const struct tm *__restrict); */
/* size_t wcslen(const wchar_t *); */
/* int wcsncasecmp(const wchar_t *, const wchar_t *, size_t); */
/* int wcsncasecmp_l(const wchar_t *, const wchar_t *, size_t, locale_t); */
/* wchar_t *wcsncat(wchar_t *__restrict, const wchar_t *__restrict, size_t); */
/* int wcsncmp(const wchar_t *, const wchar_t *, size_t); */
/* wchar_t *wcsncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t); */
/* size_t wcsnlen(const wchar_t *, size_t); */
/* size_t wcsnrtombs(char *__restrict, const wchar_t **__restrict, size_t, size_t, mbstate_t *__restrict); */
/* wchar_t *wcspbrk(const wchar_t *, const wchar_t *); */
/* wchar_t *wcsrchr(const wchar_t *, wchar_t); */
/* size_t wcsrtombs(char *__restrict, const wchar_t **__restrict, size_t, mbstate_t *__restrict); */
/* size_t wcsspn(const wchar_t *, const wchar_t *); */
/* wchar_t *wcsstr(const wchar_t *__restrict, const wchar_t *__restrict); */
/* double wcstod(const wchar_t *__restrict, wchar_t **__restrict); */
/* float wcstof(const wchar_t *__restrict, wchar_t **__restrict); */
/* wchar_t *wcstok(wchar_t *__restrict, const wchar_t *__restrict, wchar_t **__restrict); */
/* long wcstol(const wchar_t *__restrict, wchar_t **__restrict, int); */
/* long double wcstold(const wchar_t *__restrict, wchar_t **__restrict); */
/* long long wcstoll(const wchar_t *__restrict, wchar_t **__restrict, int); */
/* unsigned long wcstoul(const wchar_t *__restrict, wchar_t **__restrict, int); */
/* unsigned long long wcstoull(const wchar_t *__restrict, wchar_t **__restrict, int); */
/* int wcswidth(const wchar_t *, size_t); */
/* size_t wcsxfrm(wchar_t *__restrict, const wchar_t *__restrict, size_t); */
/* size_t wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, size_t, locale_t); */
/* int wctob(wint_t); */
/* int wcwidth(wchar_t); */
/* wchar_t *wmemchr(const wchar_t *, wchar_t, size_t); */
/* int wmemcmp(const wchar_t *, const wchar_t *, size_t); */
/* wchar_t *wmemcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t); */
/* wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t); */
/* wchar_t *wmemset(wchar_t *, wchar_t, size_t); */
/* int wprintf(const wchar_t *__restrict, ...); */
/* int wscanf(const wchar_t *__restrict, ...); */

// Needed for libcxx build.
#ifdef __cplusplus

extern wint_t btowc(int);
extern wint_t fgetwc(FILE *);
extern wchar_t *fgetws(wchar_t *__restrict, int, FILE *__restrict);
extern wint_t fputwc(wchar_t, FILE *);
extern int fputws(const wchar_t *__restrict, FILE *__restrict);
extern int fwide(FILE *, int);
extern int fwprintf(FILE *__restrict, const wchar_t *__restrict, ...);
extern int fwscanf(FILE *__restrict, const wchar_t *__restrict, ...);
extern wint_t getwc(FILE *);
extern wint_t getwchar(void);
extern size_t mbrlen(const char *__restrict, size_t, mbstate_t *__restrict);
extern size_t mbrtowc(wchar_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict);
extern int mbsinit(const mbstate_t *);
extern size_t mbsnrtowcs(wchar_t *__restrict, const char **__restrict, size_t, size_t, mbstate_t *__restrict);
extern size_t mbsrtowcs(wchar_t *__restrict, const char **__restrict, size_t, mbstate_t *__restrict);
extern FILE  *open_wmemstream(wchar_t **, size_t *);
extern wint_t putwc(wchar_t, FILE *);
extern wint_t putwchar(wchar_t);
extern int swprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, ...);
extern int swscanf(const wchar_t *__restrict, const wchar_t *__restrict, ...);
extern wint_t ungetwc(wint_t, FILE *);
extern int vfwprintf(FILE *__restrict, const wchar_t *__restrict, va_list);
extern int vfwscanf(FILE *__restrict, const wchar_t *__restrict, va_list);
extern int vswprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, va_list);
extern int vswscanf(const wchar_t *__restrict, const wchar_t *__restrict, va_list);
extern int vwprintf(const wchar_t *__restrict, va_list);
extern int vwscanf(const wchar_t *__restrict, va_list);
extern wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict);
extern wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
extern size_t wcrtomb(char *__restrict, wchar_t, mbstate_t *__restrict);
extern int wcscasecmp(const wchar_t *, const wchar_t *);
extern int wcscasecmp_l(const wchar_t *, const wchar_t *, locale_t);
extern wchar_t *wcscat(wchar_t *__restrict, const wchar_t *__restrict);
extern wchar_t *wcschr(const wchar_t *, wchar_t);
extern int wcscmp(const wchar_t *, const wchar_t *);
extern int wcscoll(const wchar_t *, const wchar_t *);
extern int wcscoll_l(const wchar_t *, const wchar_t *, locale_t);
extern wchar_t *wcscpy(wchar_t *__restrict, const wchar_t *__restrict);
extern size_t wcscspn(const wchar_t *, const wchar_t *);
extern wchar_t *wcsdup(const wchar_t *);
extern size_t wcsftime(wchar_t *__restrict, size_t, const wchar_t *__restrict, const struct tm *__restrict);
extern size_t wcslen(const wchar_t *);
extern int wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
extern int wcsncasecmp_l(const wchar_t *, const wchar_t *, size_t, locale_t);
extern wchar_t *wcsncat(wchar_t *__restrict, const wchar_t *__restrict, size_t);
extern int wcsncmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t *wcsncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
extern size_t wcsnlen(const wchar_t *, size_t);
extern size_t wcsnrtombs(char *__restrict, const wchar_t **__restrict, size_t, size_t, mbstate_t *__restrict);
extern wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
extern wchar_t *wcsrchr(const wchar_t *, wchar_t);
extern size_t wcsrtombs(char *__restrict, const wchar_t **__restrict, size_t, mbstate_t *__restrict);
extern size_t wcsspn(const wchar_t *, const wchar_t *);
extern wchar_t *wcsstr(const wchar_t *__restrict, const wchar_t *__restrict);
extern double wcstod(const wchar_t *__restrict, wchar_t **__restrict);
extern float wcstof(const wchar_t *__restrict, wchar_t **__restrict);
extern wchar_t *wcstok(wchar_t *__restrict, const wchar_t *__restrict, wchar_t **__restrict);
extern long wcstol(const wchar_t *__restrict, wchar_t **__restrict, int);
extern long double wcstold(const wchar_t *__restrict, wchar_t **__restrict);
extern long long wcstoll(const wchar_t *__restrict, wchar_t **__restrict, int);
extern unsigned long wcstoul(const wchar_t *__restrict, wchar_t **__restrict, int);
extern unsigned long long wcstoull(const wchar_t *__restrict, wchar_t **__restrict, int);
extern int wcswidth(const wchar_t *, size_t);
extern size_t wcsxfrm(wchar_t *__restrict, const wchar_t *__restrict, size_t);
extern size_t wcsxfrm_l(wchar_t *__restrict, const wchar_t *__restrict, size_t, locale_t);
extern int wctob(wint_t);
extern int wcwidth(wchar_t);
extern wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
extern int wmemcmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t *wmemcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
extern wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
extern wchar_t *wmemset(wchar_t *, wchar_t, size_t);
extern int wprintf(const wchar_t *__restrict, ...);
extern int wscanf(const wchar_t *__restrict, ...);

#endif

#ifdef __cplusplus
}
#endif
