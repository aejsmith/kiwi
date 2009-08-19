/* Kiwi C library - Standard library functions
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

extern void _Exit(int status) __attribute__((noreturn));
/* long a64l(const char *); */
extern void abort(void);
//extern int abs(int j);
extern int atexit(void (*function)(void));
//extern double atof(const char *s);
//extern int atoi(const char *s);
//extern long atol(const char *s);
//extern long long atoll(const char *s);
//extern void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
extern void *calloc(size_t nmemb, size_t size);
/* div_t         div(int, int); */
extern void exit(int status) __attribute__((noreturn));
extern void free(void *ptr);
extern char *getenv(const char *name);
//extern long labs(long j);
/* ldiv_t        ldiv(long, long); */
//extern long long llabs(long long j);
/* lldiv_t       lldiv(long long, long long); */
extern void *malloc(size_t size);
/* int           mblen(const char *, size_t); */
/* size_t        mbstowcs(wchar_t *restrict, const char *restrict, size_t); */
/* int           mbtowc(wchar_t *restrict, const char *restrict, size_t); */
//extern char *mktemp(char *tpl);
//extern int mkstemp(char *tpl);
/* long          mrand48(void); */
/* long          nrand48(unsigned short[3]); */
//extern int posix_memalign(void **memptr, size_t alignment, size_t size);
/* int           posix_openpt(int); */
/* char         *ptsname(int); */
//extern int putenv(char *str);
//extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
//extern int rand(void);
//extern int rand_r(unsigned int *seed);
extern void *realloc(void *ptr, size_t size);
//extern int setenv(const char *name, const char *value, int overwrite);
//extern void srand(unsigned int seed);

//extern double strtod(const char *s, char **endptr);
/* float         strtof(const char *restrict, char **restrict); */
extern long strtol(const char *cp, char **endp, int base);
/* long double   strtold(const char *restrict, char **restrict); */
extern long long int strtoll(const char *cp, char **endp, int base);
extern unsigned long strtoul(const char *cp, char **endp, int base);
extern unsigned long long int strtoull(const char *cp, char **endp, int base);

/* int           system(const char *); */
/* int           unlockpt(int); */
/* int           unsetenv(const char *); */
/* size_t        wcstombs(char *restrict, const wchar_t *restrict, size_t); */
/* int           wctomb(char *, wchar_t); */

#ifdef __cplusplus
}
#endif

#endif /* __STDLIB_H */
