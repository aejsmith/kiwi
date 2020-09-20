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
 * @brief               Standard library functions.
 */

#pragma once

#define __need_size_t
#define __need_wchar_t
#define __need_NULL
#include <stddef.h>

#include <sys/wait.h>

#include <system/defs.h>

#include <locale.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAND_MAX        32767
#define ATEXIT_MAX      32
#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

#define MB_CUR_MAX      1

/** Structure containing result from div(). */
typedef struct {
    int quot, rem;
} div_t;

/** Structure containing result from ldiv(). */
typedef struct {
    long quot, rem;
} ldiv_t;

/** Structure containing result from lldiv(). */
typedef struct {
    long long quot, rem;
} lldiv_t;

extern void _Exit(int status) __sys_noreturn;
extern void abort(void) __sys_noreturn;
extern int abs(int j);
extern int atexit(void (*func)(void));
extern double atof(const char *s);
extern int atoi(const char *s);
extern long atol(const char *s);
extern long long atoll(const char *s);
extern void *bsearch(
    const void *key, const void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *));
extern void *calloc(size_t nmemb, size_t size);
extern div_t div(int num, int denom);
extern void exit(int status) __sys_noreturn;
extern void free(void *ptr);
extern char *getenv(const char *name);
//extern int getsubopt(char **optionp, char *const *keylistp, char **valuep);
extern long labs(long j);
extern ldiv_t ldiv(long num, long denom);
extern long long llabs(long long j);
//extern lldiv_t lldiv(long long numerator, long long denominator);
extern void *malloc(size_t size);
//extern int mblen(const char *s, size_t n);
//extern size_t mbstowcs(wchar_t *__restrict dest, const char *__restrict src, size_t n);
//extern int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
extern char *mktemp(char *tpl);
//extern char *mkdtemp(char *tpl);
extern int mkstemp(char *tpl);
extern int posix_memalign(void **memptr, size_t alignment, size_t size);
extern int putenv(char *str);
extern void qsort(
    void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *));
extern int rand(void);
extern int rand_r(unsigned int *seed);
extern void *realloc(void *ptr, size_t size);
extern int setenv(const char *name, const char *value, int overwrite);
extern void srand(unsigned int seed);
extern double strtod(const char *__restrict s, char **__restrict endptr);
//extern float strtof(const char *__restrict s, char **__restrict endptr);
extern long strtol(const char *__restrict cp, char **__restrict endp, int base);
//extern long double strtold(const char *__restrict str, char **__restrict endptr);
extern long long int strtoll(const char *__restrict cp, char **__restrict endp, int base);
extern unsigned long strtoul(const char *__restrict cp, char **__restrict endp, int base);
extern unsigned long long int strtoull(const char *__restrict cp, char **__restrict endp, int base);
extern int system(const char *command);
extern int unsetenv(const char *name);
//extern size_t wcstombs(char *__restrict dest, const wchar_t *__restrict src, size_t n);
//extern int wctomb(char *s, wchar_t wc);

// Needed for libcxx build.
#ifdef __cplusplus

extern float strtof(const char *__restrict s, char **__restrict endptr);
extern long double strtold(const char *__restrict str, char **__restrict endptr);
extern lldiv_t lldiv(long long numerator, long long denominator);
extern int mblen(const char *s, size_t n);
extern int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
extern int wctomb(char *s, wchar_t wc);
extern size_t mbstowcs(wchar_t *__restrict dest, const char *__restrict src, size_t n);
extern size_t wcstombs(char *__restrict dest, const wchar_t *__restrict src, size_t n);
extern long strtol_l(const char *__restrict, char **__restrict, int, locale_t);
extern long long strtoll_l(const char *__restrict, char **__restrict, int, locale_t);
extern unsigned long strtoul_l(const char *__restrict, char **__restrict, int, locale_t);
extern unsigned long long strtoull_l(const char *__restrict, char **__restrict, int, locale_t);
extern double strtod_l(const char *__restrict, char **__restrict, locale_t);
extern float strtof_l(const char *__restrict, char **__restrict, locale_t);
extern long double strtold_l(const char *__restrict, char **__restrict, locale_t);

#endif

#ifdef __cplusplus
}
#endif
