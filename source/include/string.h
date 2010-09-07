/*
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
 * @brief		String functions.
 */

#ifndef __STRING_H
#define __STRING_H

#define __need_size_t
#define __need_NULL
#include <stddef.h>

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
extern char *strcpy(char *__restrict dest, const char *__restrict src);
extern size_t strcspn(const char *s, const char *reject);
extern char *strdup(const char *s);
extern char *strerror(int err);
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
extern size_t strspn(const char *s, const char *accept);
extern char *strstr(const char *haystack, const char *needle);
extern char *strtok(char *__restrict str, const char *__restrict delim);
extern char *strtok_r(char *__restrict str, const char *__restrict delim, char **__restrict saveptr);
extern size_t strxfrm(char *__restrict dest, const char *__restrict src, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* __STRING_H */
