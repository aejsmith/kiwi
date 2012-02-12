/*
 * Copyright (C) 2007-2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		String handling functions.
 */

#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include <mm/mm.h>

#include <types.h>

extern void *memcpy(void *restrict dest, const void *restrict src, size_t count);
extern void *memset(void *dest, int val, size_t count);
extern void *memmove(void *dest, const void *src, size_t count);
extern int memcmp(const void *p1, const void *p2, size_t count);
extern size_t strlen(const char *str);
extern size_t strnlen(const char *str, size_t count);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t count);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t count);
extern char *strsep(char **stringp, const char *delim);
extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);
extern char *strstr(const char *s, const char *what);
extern char *strstrip(char *str);
extern char *strcpy(char *restrict dest, const char *restrict src);
extern char *strncpy(char *restrict dest, const char *restrict src, size_t count);
extern char *strcat(char *restrict dest, const char *restrict src);
extern void *kmemdup(const void *src, size_t count, int mmflag);
extern char *kstrdup(const char *s, int mmflag);
extern char *kstrndup(const char *s, size_t n, int mmflag);
extern char *kbasename(const char *path, int mmflag);
extern char *kdirname(const char *path, int mmflag);
extern unsigned long strtoul(const char *cp, char **endp, unsigned int base);
extern long strtol(const char *cp,char **endp,unsigned int base);
extern unsigned long long strtoull(const char *cp, char **endp, unsigned int base);
extern long long strtoll(const char *cp, char **endp, unsigned int base);
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
extern int vsprintf(char *buf, const char *fmt, va_list args);
extern int snprintf(char *buf, size_t size, const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);

#endif /* __LIB_STRING_H */
