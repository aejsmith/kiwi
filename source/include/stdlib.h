/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Standard library functions.
 */

#ifndef __STDLIB_H
#define __STDLIB_H

#define __need_size_t
#define __need_wchar_t
#define __need_NULL
#include <stddef.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RAND_MAX	32767
#define ATEXIT_MAX	32
#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1

#define MB_CUR_MAX	1

/** Structure containing result from div(). */
typedef struct {
	int quot, rem;
} div_t;

/** Structure containing result from ldiv(). */
typedef struct {
	long int quot, rem;
} ldiv_t;

extern void _Exit(int status) __attribute__((noreturn));
extern void abort(void);
extern int abs(int j);
extern int atexit(void (*func)(void));
extern double atof(const char *s);
extern int atoi(const char *s);
extern long atol(const char *s);
extern long long atoll(const char *s);
extern void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
extern void *calloc(size_t nmemb, size_t size);
extern div_t div(int num, int denom);
extern void exit(int status) __attribute__((noreturn));
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
extern int putenv(char *str);
extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
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
//extern int system(const char *command);
extern int unsetenv(const char *name);
//extern size_t wcstombs(char *__restrict dest, const wchar_t *__restrict src, size_t n);
//extern int wctomb(char *s, wchar_t wc);

#ifdef __cplusplus
extern int system(const char *command);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STDLIB_H */
