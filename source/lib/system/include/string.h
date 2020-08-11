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
 * @brief               String functions.
 */

#ifndef __STRING_H
#define __STRING_H

#define __need_size_t
#define __need_NULL
#include <stddef.h>

#include <locale.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int ffs(int i);
extern void *memchr(const void *s, int c, size_t n);
extern int memcmp(const void *p1, const void *p2, size_t count);
extern void *memcpy(void *__restrict dest, const void *__restrict src, size_t count);
extern void *memmove(void *dest, const void *src, size_t count);
extern void *memset(void *dest, int val, size_t count);
/* char *stpcpy(char *__restrict, const char *__restrict); */
/* char *stpncpy(char *__restrict, const char *__restrict, size_t); */
extern int strcasecmp(const char *s1, const char *s2);
extern char *strcat(char *__restrict dest, const char *__restrict src);
extern char *strchr(const char *t, int c);
extern int strcmp(const char *s, const char *t);
extern int strcoll(const char *s1, const char *s2);
/* int strcoll_l(const char *, const char *, locale_t); */
extern char *strcpy(char *__restrict dest, const char *__restrict src);
extern size_t strcspn(const char *s, const char *reject);
extern char *strdup(const char *s);
extern char *strerror(int err);
/* char *strerror_l(int, locale_t); */
extern size_t strlen(const char *str);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
extern char *strncat(char *__restrict dest, const char *__restrict src, size_t max);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern char *strncpy(char *__restrict dest, const char *__restrict src, size_t n);
extern char *strndup(const char *s, size_t n);
extern size_t strnlen(const char *str, size_t count);
extern char *strpbrk(const char *s, const char *accept);
extern char *strrchr(const char *t, int c);
extern char *strsep(char **stringp, const char *delim);
extern char *strsignal(int sig);
extern size_t strspn(const char *s, const char *accept);
extern char *strstr(const char *haystack, const char *needle);
extern char *strtok(char *__restrict str, const char *__restrict delim);
extern char *strtok_r(char *__restrict str, const char *__restrict delim, char **__restrict saveptr);
extern size_t strxfrm(char *__restrict dest, const char *__restrict src, size_t count);
/* size_t strxfrm_l(char *__restrict, const char *__restrict, size_t, locale_t); */

#ifdef __cplusplus

extern int strcoll_l(const char *, const char *, locale_t);
extern size_t strxfrm_l(char *__restrict, const char *__restrict, size_t, locale_t);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __STRING_H */
