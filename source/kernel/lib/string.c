/*
 * Copyright (C) 2007-2012 Alex Smith
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
 * @brief		String handling functions.
 */

#include <lib/ctype.h>
#include <lib/printf.h>
#include <lib/string.h>

#include <mm/malloc.h>

/**
 * Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @note		This function does not like unaligned addresses. Giving
 *			it unaligned addresses might make it sad. :(
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
	const char *s = (const char *)src;
	const unsigned long *ns;
	char *d = (char *)dest;
	unsigned long *nd;

	/* Align the destination. */
	while((ptr_t)d & (sizeof(unsigned long) - 1)) {
		if(count--) {
			*d++ = *s++;
		} else {
			return dest;
		}
	}

	/* Write in native-sized blocks if we can. */
	if(count >= sizeof(unsigned long)) {
		nd = (unsigned long *)d;
		ns = (const unsigned long *)s;

		/* Unroll the loop if possible. */
		while(count >= (sizeof(unsigned long) * 4)) {
			*nd++ = *ns++;
			*nd++ = *ns++;
			*nd++ = *ns++;
			*nd++ = *ns++;
			count -= sizeof(unsigned long) * 4;
		}
		while(count >= sizeof(unsigned long)) {
			*nd++ = *ns++;
			count -= sizeof(unsigned long);
		}

		d = (char *)nd;
		s = (const char *)ns;
	}

	/* Write remaining bytes. */
	while(count--)
		*d++ = *s++;

	return dest;
}

/** Fill a memory area.
 * @param dest		The memory area to fill.
 * @param val		The value to fill with (converted to an unsigned char).
 * @param count		The number of bytes to fill.
 * @return		Destination location. */
void *memset(void *dest, int val, size_t count) {
	unsigned char c = val & 0xff;
	unsigned long *nd, nval;
	char *d = (char *)dest;

	/* Align the destination. */
	while((ptr_t)d & (sizeof(unsigned long) - 1)) {
		if(count--) {
			*d++ = c;
		} else {
			return dest;
		}
	}

	/* Write in native-sized blocks if we can. */
	if(count >= sizeof(unsigned long)) {
		nd = (unsigned long *)d;

		/* Compute the value we will write. */
		#if CONFIG_64BIT
		nval = c * 0x0101010101010101ul;
		#elif CONFIG_32BIT
		nval = c * 0x01010101ul;
		#endif

		/* Unroll the loop if possible. */
		while(count >= (sizeof(unsigned long) * 4)) {
			*nd++ = nval;
			*nd++ = nval;
			*nd++ = nval;
			*nd++ = nval;
			count -= sizeof(unsigned long) * 4;
		}
		while(count >= sizeof(unsigned long)) {
			*nd++ = nval;
			count -= sizeof(unsigned long);
		}

		d = (char *)nd;
	}

	/* Write remaining bytes. */
	while(count--)
		*d++ = val;

	return dest;
}

/**
 * Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memmove(void *dest, const void *src, size_t count) {
	const char *b = src;
	char *a = dest;

	if(src != dest) {
		if(src > dest) {
			memcpy(dest, src, count);
		} else {
			a += count - 1;
			b += count - 1;
			while(count--) {
				*a-- = *b--;
			}
		}
	}

	return dest;
}

/** Compare 2 chunks of memory.
 * @param p1		Pointer to the first chunk.
 * @param p2		Pointer to the second chunk.
 * @param count		Number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			p1 is found, respectively, to be less than, to match,
 *			or to be greater than p2. */
int memcmp(const void *p1, const void *p2, size_t count) {
	const unsigned char *s1 = (const unsigned char *)p1;
	const unsigned char *s2 = (const unsigned char *)p2;

	while(count--) {
		if(*s1 != *s2)
			return *s1 - *s2;

		s1++;
		s2++;
	}

	return 0;
}

/**
 * Get the length of string.
 *
 * Gets the length of the string specified. The length is the number of
 * characters found before a NULL byte.
 *
 * @param str		Pointer to the string.
 *
 * @return		Length of the string.
 */
size_t strlen(const char *str) {
	size_t ret;

	for(ret = 0; *str != '\0'; ret++, str++);
	return ret;
}

/**
 * Get length of string with limit.
 *
 * Gets the length of the string specified. The length is the number of
 * characters found either before a NULL byte or before the maximum length
 * specified.
 *
 * @param str		Pointer to the string.
 * @param count		Maximum length of the string.
 *
 * @return		Length of the string.
 */
size_t strnlen(const char *str, size_t count) {
	size_t ret;

	for(ret = 0; *str != '\0' && ret < count; ret++, str++);
	return ret;
}

/** Compare two strings.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
	for(; *s1 && *s2 && *s1 == *s2; s1++, s2++);
	return *s1 - *s2;
}

/** Compare two strings with a length limit.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1, *b = s2, *fini = a + count;
	int res;

	while(a < fini) {
		res = *a - *b;
		if(res)
			return res;
		if(!*a)
			return 0;

		a++;
		b++;
	}
	return 0;
}

/** Compare two strings ignorning case.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strcasecmp(const char *s1, const char *s2) {
	for(; *s1 && *s2 && tolower(*s1) == tolower(*s2); s1++, s2++);
	return tolower(*s1) - tolower(*s2);
}

/** Compare two strings with a length limit ignoring case.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strncasecmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1, *b = s2, *fini = a + count;
	int res;

	while(a < fini) {
		res = tolower(*a) - tolower(*b);
		if(res)
			return res;
		if(!*a)
			return 0;

		a++;
		b++;
	}
	return 0;
}

/**
 * Separate a string.
 *
 * Finds the first occurrence of a symbol in the string delim in *stringp.
 * If one is found, the delimeter is replaced by a NULL byte and the pointer
 * pointed to by stringp is updated to point past the string. If no delimeter
 * is found *stringp is made NULL and the token is taken to be the entire
 * string.
 *
 * @param stringp	Pointer to a pointer to the string to separate.
 * @param delim		String containing all possible delimeters.
 *
 * @return		NULL if stringp is NULL, otherwise a pointer to the
 *			token found.
 */
char *strsep(char **stringp, const char *delim) {
	const char *spanp;
	char *tok, *s;
	int c, sc;

	if(!(s = *stringp))
		return s;

	for(tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if((sc = *spanp++) == c) {
				if(c == 0) {
					s = NULL;
				} else {
					s[-1] = 0;
				}

				*stringp = s;
				return tok;
			}
		} while(sc != 0);
	}
}

/** Find the first occurrence of a character in a string.
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * @return		NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
	char ch = c;

	for (;;) {
		if(*s == ch) {
			break;
		} else if(!*s) {
			return NULL;
		} else {
			s++;
		}
	}

	return (char *)s;
}

/** Find the last occurrence of a character in a string.
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * @return		NULL if token not found, otherwise pointer to token. */
char *strrchr(const char *s, int c) {
	const char *l = NULL;

	for(;;) {
		if(*s == c)
			l = s;
		if(!*s)
			return (char *)l;

		s++;
	}

	return (char *)l;
}

/** Find the first occurrence of a substring in a string.
 * @param s		String to search.
 * @param what		Substring to search for.
 * @return		Pointer to start of match if found, null if not. */
char *strstr(const char *s, const char *what) {
	size_t len = strlen(what);

	while(*s) {
		if(strncmp(s, what, len) == 0)
			return (char *)s;

		s++;
	}

	return NULL;
}

/**
 * Strip whitespace from a string.
 *
 * Strips whitespace from the start and end of a string. The string is modified
 * in-place.
 *
 * @param str           String to remove from.
 *
 * @return              Pointer to new start of string.
 */
char *strstrip(char *str) {
	size_t len;

	/* Strip from beginning. */
	while(isspace(*str))
		str++;

	/* Strip from end. */
	len = strlen(str);
	while(len--) {
		if(!isspace(str[len]))
			break;
	}
	str[++len] = 0;
	return str;
}

/**
 * Copy a string.
 *
 * Copies a string from one place to another. Assumes that the destination
 * is big enough to hold the string.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 *
 * @return		The value specified for dest.
 */
char *strcpy(char *restrict dest, const char *restrict src) {
	char *d = dest;

	while((*d++ = *src++));
	return dest;
}

/**
 * Copy a string with a length limit.
 *
 * Copies a string from one place to another. Will copy at most the number
 * of bytes specified.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * @param count		Maximum number of bytes to copy.
 *
 * @return		The value specified for dest.
 */
char *strncpy(char *restrict dest, const char *restrict src, size_t count) {
	size_t i;

	for(i = 0; i < count; i++) {
		dest[i] = src[i];
		if(!src[i])
			break;
	}

	return dest;
}

/** Concatenate two strings.
 * @param dest		Pointer to the string to append to. The containing
 *			buffer must have enough extra space for the source
 *			string.
 * @param src		Pointer to the string to append.
 * @return		Pointer to dest. */
char *strcat(char *restrict dest, const char *restrict src) {
	size_t destlen = strlen(dest);
	char *d = dest + destlen;

	while((*d++ = *src++));
	return dest;
}

/**
 * Duplicate memory.
 *
 * Allocates a block of memory big enough and copies the source to it. The
 * memory returned should be freed with kfree().
 *
 * @param src		Memory to duplicate.
 * @param count		Number of bytes to duplicate.
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Pointer to duplicated memory.
 */
void *kmemdup(const void *src, size_t count, unsigned mmflag) {
	char *dest;

	if(count == 0)
		return NULL;

	dest = kmalloc(count, mmflag);
	if(dest)
		memcpy(dest, src, count);

	return dest;
}

/**
 * Duplicate a string.
 *
 * Allocates a buffer big enough to hold the given string and copies the
 * string to it. The memory returned should be freed with kfree().
 *
 * @param src		Pointer to the source buffer.
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Pointer to the allocated buffer containing the string.
 */
char *kstrdup(const char *src, unsigned mmflag) {
	size_t len = strlen(src) + 1;
	char *dup;

	dup = kmalloc(len, mmflag);
	if(dup)
		memcpy(dup, src, len);

	return dup;
}

/**
 * Duplicate a string with a length limit.
 *
 * Allocates a buffer either as big as the string or the maximum length
 * given, and then copies at most the number of bytes specified of the string
 * to it. If the string is longer than the limit, a null byte will be added
 * to the end of the duplicate. The memory returned should be freed with
 * kfree().
 *
 * @param src		Pointer to the source buffer.
 * @param n		Maximum number of bytes to copy.
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Pointer to the allocated buffer containing the string.
 */
char *kstrndup(const char *src, size_t n, unsigned mmflag) {
	size_t len;
	char *dup;

	len = strnlen(src, n);
	dup = kmalloc(len + 1, mmflag);
	if(dup) {
		memcpy(dup, src, len);
		dup[len] = '\0';
	}

	return dup;
}

/**
 * Get the last component of a path.
 *
 * Returns an allocated string buffer containing the last component of the
 * given path.
 *
 * @param path		Pathname to parse.
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Pointer to string containing last component of path.
 *			The string returned is allocated via kmalloc(), so
 *			should be freed using kfree().
 */
char *kbasename(const char *path, unsigned mmflag) {
	char *ptr, *dup, *ret;
	size_t len;

	if(path == NULL || path[0] == 0 || (path[0] == '.' && path[1] == 0)) {
		return kstrdup(".", mmflag);
	} else if(path[0] == '.' && path[1] == '.' && path[2] == 0) {
		return kstrdup("..", mmflag);
	}

	if(!(dup = kstrdup(path, mmflag)))
		return NULL;

	/* Strip off trailing '/' characters. */
	len = strlen(dup);
	while(len && dup[len - 1] == '/')
		dup[--len] = 0;

	/* If length is now 0, the entire string was '/' characters. */
	if(!len) {
		kfree(dup);
		return kstrdup("/", mmflag);
	}

	if(!(ptr = strrchr(dup, '/'))) {
		/* No '/' character in the string, that means what we have is
		 * correct. Resize the allocation to the new length. */
		if(!(ret = krealloc(dup, len + 1, mmflag)))
			kfree(dup);

		return ret;
	} else {
		ret = kstrdup(ptr + 1, mmflag);
		kfree(dup);
		return ret;
	}
}

/**
 * Get the part of a path preceding the last /.
 *
 * Returns an allocated string buffer containing everything preceding the last
 * component of the given path.
 *
 * @param path		Pathname to parse.
 * @param mmflag	Allocation behaviour flags.
 *
 * @return		Pointer to string. The string returned is allocated via
 *			kmalloc(), so should be freed using kfree().
 */
char *kdirname(const char *path, unsigned mmflag) {
	char *ptr, *dup, *ret;
	size_t len;

	if(path == NULL || path[0] == 0 || (path[0] == '.' && path[1] == 0)) {
		return kstrdup(".", mmflag);
	} else if(path[0] == '.' && path[1] == '.' && path[2] == 0) {
		return kstrdup(".", mmflag);
	}

	/* Duplicate string to modify it. */
	if(!(dup = kstrdup(path, mmflag)))
		return NULL;

	/* Strip off trailing '/' characters. */
	len = strlen(dup);
	while(len && dup[len - 1] == '/')
		dup[--len] = 0;

	/* Look for last '/' character. */
	if(!(ptr = strrchr(dup, '/'))) {
		kfree(dup);
		return kstrdup(".", mmflag);
	}

	/* Strip off the character and any extras. */
	len = (ptr - dup) + 1;
	while(len && dup[len - 1] == '/')
		dup[--len] = 0;

	if(!len) {
		kfree(dup);
		return kstrdup("/", mmflag);
	} else if(!(ret = krealloc(dup, len + 1, mmflag))) {
		kfree(dup);
	}

	return ret;
}

/** Macro to implement strtoul() and strtoull(). */
#define __strtoux(type, cp, endp, base)		\
	__extension__ \
	({ \
		type result = 0, value; \
		if(!base) { \
			if(*cp == '0') { \
				if((tolower(*(++cp)) == 'x') && isxdigit(cp[1])) { \
					cp++; \
					base = 16; \
				} else { \
					base = 8; \
				} \
			} else { \
				base = 10; \
			} \
		} else if(base == 16) { \
			if(cp[0] == '0' && tolower(cp[1]) == 'x') { \
				cp += 2; \
			} \
		} \
		while(isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : tolower(*cp) - 'a' + 10) < base) { \
			result = result * base + value; \
			cp++; \
		} \
		if(endp) { \
			*endp = (char *)cp; \
		} \
		result; \
	})

/** Convert a string to an unsigned long.
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use (if zero will guess).
 * @return		Converted value. */
unsigned long strtoul(const char *cp, char **endp, unsigned int base) {
	return __strtoux(unsigned long, cp, endp, base);
}

/** Convert a string to a signed long.
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 * @return		Converted value. */
long strtol(const char *cp, char **endp, unsigned int base) {
	if(*cp == '-') {
		return -strtoul(cp + 1, endp, base);
	}
	return strtoul(cp, endp, base);
}

/** Convert a string to an unsigned long long.
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 * @return		Converted value. */
unsigned long long strtoull(const char *cp, char **endp, unsigned int base) {
	return __strtoux(unsigned long long, cp, endp, base);
}

/** Convert a string to an signed long long.
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 * @return		Converted value. */
long long strtoll(const char *cp, char **endp, unsigned int base) {
	if(*cp == '-') {
		return -strtoull(cp + 1, endp, base);
	}
	return strtoull(cp, endp, base);
}

/** Data used by vsnprintf_helper(). */
struct vsnprintf_data {
	char *buf;			/**< Buffer to write to. */
	size_t size;			/**< Total size of buffer. */
	size_t off;			/**< Current number of bytes written. */
};

/** Helper for vsnprintf().
 * @param ch		Character to place in buffer.
 * @param _data		Data.
 * @param total		Pointer to total character count. */
static void vsnprintf_helper(char ch, void *_data, int *total) {
	struct vsnprintf_data *data = _data;

	if(data->off < data->size) {
		data->buf[data->off++] = ch;
		*total = *total + 1;
	}
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL.
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	struct vsnprintf_data data;
	int ret;

	data.buf = buf;
	data.size = size - 1;
	data.off = 0;

	ret = do_vprintf(vsnprintf_helper, &data, fmt, args);

	if(data.off < data.size) {
		data.buf[data.off] = 0;
	} else {
		data.buf[data.size-1] = 0;
	}
	return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given. The buffer must be large enough to store the formatted
 * string; it is preferrable to use vsnprintf().
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL.
 */
int vsprintf(char *buf, const char *fmt, va_list args) {
	return vsnprintf(buf, (size_t)-1, fmt, args);
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return ret;
}

/**
 * Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given. The buffer must be large enough to store the formatted
 * string; it is preferrable to use snprintf().
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int sprintf(char *buf, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsprintf(buf, fmt, args);
	va_end(args);

	return ret;
}
