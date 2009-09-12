/* String functions
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

#ifdef __cplusplus
extern "C" {
#endif

/* Get size_t and NULL from stddef.h. */
#define __need_size_t
#define __need_NULL
#include <stddef.h>

extern void *memchr(const void *s, int c, size_t n);
extern int memcmp(const void *p1, const void *p2, size_t count);
extern void *memcpy(void *dest, const void *src, size_t count);
extern void *memmove(void *dest, const void *src, size_t count);
extern void *memset(void *dest, int val, size_t count);
extern size_t strlen(const char *str);
extern size_t strnlen(const char *str, size_t count);
extern int strcmp(const char *s, const char *t);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern char *strsep(char **stringp, const char *delim);
extern char *strchr(const char *t, int c);
extern char *strrchr(const char *t, int c);
extern char *strstr(const char *haystack, const char *needle);
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *dest, const char *src, size_t n);
extern char *strdup(const char *s);
extern char *strndup(const char *s, size_t n);
extern char *strcat(char *dest, const char *src);
extern char *strncat(char *dest, const char *src, size_t max);
/* int strcoll(const char *, const char *); */
extern size_t strcspn(const char *s, const char *reject);
extern char *strpbrk(const char *s, const char *accept);
extern size_t strspn(const char *s, const char *accept);
extern char *strtok(char *str, const char *delim);
extern char *strtok_r(char *str, const char *delim, char **saveptr);
/* size_t strxfrm(char *, const char *, size_t); */
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __STRING_H */
