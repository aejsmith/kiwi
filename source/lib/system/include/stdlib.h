/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Standard library functions.
 */

#pragma once

#define __NEED_locale_t
#define __NEED_NULL
#define __NEED_size_t
#define __NEED_wchar_t
#include <bits/alltypes.h>

#include <sys/wait.h>

__SYS_EXTERN_C_BEGIN

#define RAND_MAX        0x7fffffff
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
extern int mblen(const char *s, size_t n);
extern size_t mbstowcs(wchar_t *__restrict dest, const char *__restrict src, size_t n);
extern int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
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
extern char *realpath(const char *__restrict file_name, char *__restrict resolved_name);
extern int setenv(const char *name, const char *value, int overwrite);
extern void srand(unsigned int seed);
extern double strtod(const char *__restrict s, char **__restrict endptr);
//extern float strtof(const char *__restrict s, char **__restrict endptr);
extern long strtol(const char *__restrict cp, char **__restrict endp, int base);
extern long double strtold(const char *__restrict s, char **__restrict endptr);
extern long long int strtoll(const char *__restrict cp, char **__restrict endp, int base);
extern unsigned long strtoul(const char *__restrict cp, char **__restrict endp, int base);
extern unsigned long long int strtoull(const char *__restrict cp, char **__restrict endp, int base);
extern int system(const char *command);
extern int unsetenv(const char *name);
extern size_t wcstombs(char *__restrict dest, const wchar_t *__restrict src, size_t n);
extern int wctomb(char *s, wchar_t wc);

/** Helper for __sys_cleanup_free. */
static inline void __sys_freep(void *p) {
    free(*(void **)p);
}

/** Attribute to free a pointer with free() when it goes out of scope. */
#define __sys_cleanup_free  __sys_cleanup(__sys_freep)

// Needed for libcxx build.
#ifdef __cplusplus

extern float strtof(const char *__restrict s, char **__restrict endptr);
extern lldiv_t lldiv(long long numerator, long long denominator);
extern long strtol_l(const char *__restrict, char **__restrict, int, locale_t);
extern long long strtoll_l(const char *__restrict, char **__restrict, int, locale_t);
extern unsigned long strtoul_l(const char *__restrict, char **__restrict, int, locale_t);
extern unsigned long long strtoull_l(const char *__restrict, char **__restrict, int, locale_t);
extern double strtod_l(const char *__restrict, char **__restrict, locale_t);
extern float strtof_l(const char *__restrict, char **__restrict, locale_t);
extern long double strtold_l(const char *__restrict, char **__restrict, locale_t);

#endif

__SYS_EXTERN_C_END
