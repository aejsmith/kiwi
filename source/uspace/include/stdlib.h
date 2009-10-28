/* Standard library functions
 * Copyright (C) 2008-2009 Alex Smith
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

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#define __need_wchar_t
#define __need_NULL
#include <stddef.h>

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
extern int atexit(void (*function)(void));
extern double atof(const char *s);
extern int atoi(const char *s);
extern long atol(const char *s);
extern long long atoll(const char *s);
extern void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
extern void *calloc(size_t nmemb, size_t size);
//extern div_t div(int numerator, int denominator);
extern void exit(int status) __attribute__((noreturn));
extern void free(void *ptr);
extern char *getenv(const char *name);
extern long labs(long j);
//extern ldiv_t ldiv(long numerator, long denominator);
extern long long llabs(long long j);
//extern lldiv_t lldiv(long long numerator, long long denominator);
extern void *malloc(size_t size);
//extern int mblen(const char *s, size_t n);
//extern size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
//extern int mbtowc(wchar_t *pwc, const char *s, size_t n);
//extern char *mktemp(char *tpl);
//extern int mkstemp(char *tpl);
extern int putenv(char *str);
extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
extern int rand(void);
extern int rand_r(unsigned int *seed);
extern void *realloc(void *ptr, size_t size);
extern int setenv(const char *name, const char *value, int overwrite);
extern void srand(unsigned int seed);
extern double strtod(const char *s, char **endptr);
//extern float strtof(const char *s, char **endptr);
extern long strtol(const char *cp, char **endp, int base);
//extern long double strtold(const char *str, char **endptr);
extern long long int strtoll(const char *cp, char **endp, int base);
extern unsigned long strtoul(const char *cp, char **endp, int base);
extern unsigned long long int strtoull(const char *cp, char **endp, int base);
//extern int system(const char *command);
//extern int unsetenv(const char *name);
//extern size_t wcstombs(char *dest, const wchar_t *src, size_t n);
//extern int wctomb(char *s, wchar_t wc);

#ifdef __cplusplus
extern div_t div(int numerator, int denominator);
extern ldiv_t ldiv(long numerator, long denominator);
extern int system(const char *command);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STDLIB_H */
