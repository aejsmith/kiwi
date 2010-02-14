/*
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		String handling functions.
 */

#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#ifndef LOADER
# include <mm/flags.h>
#endif

#include <stdarg.h>
#include <types.h>

extern void *memcpy(void *restrict dest, const void *restrict src, size_t count);
extern void *memset(void *dest, int val, size_t count);
extern void *memmove(void *dest, const void *src, size_t count);
extern size_t strlen(const char *str);
extern size_t strnlen(const char *str, size_t count);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t count);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t count);
extern char *strsep(char **stringp, const char *delim);
extern char *strchr(const char *s, int c);
extern char *strrchr(const char *s, int c);
extern char *strstrip(char *str);
extern char *strcpy(char *restrict dest, const char *restrict src);
extern char *strncpy(char *restrict dest, const char *restrict src, size_t count);
extern char *strcat(char *restrict dest, const char *restrict src);
#ifdef LOADER
extern char *kstrdup(const char *s);
extern char *kstrndup(const char *s, size_t n);
#else
extern void *kmemdup(const void *src, size_t count, int kmflag);
extern char *kstrdup(const char *s, int kmflag);
extern char *kstrndup(const char *s, size_t n, int kmflag);
extern char *kbasename(const char *path, int kmflag);
extern char *kdirname(const char *path, int kmflag);
#endif
extern unsigned long strtoul(const char *cp, char **endp, unsigned int base);
extern long strtol(const char *cp,char **endp,unsigned int base);
extern unsigned long long strtoull(const char *cp, char **endp, unsigned int base);
extern long long strtoll(const char *cp, char **endp, unsigned int base);
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
extern int vsprintf(char *buf, const char *fmt, va_list args);
extern int snprintf(char *buf, size_t size, const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);

#endif /* __LIB_STRING_H */
