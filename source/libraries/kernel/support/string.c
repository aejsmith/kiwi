/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kernel library support functions.
 */

#include <stdlib.h>
#include <string.h>

/** Get length of string.
 * @param str		Pointer to the string.
 * @return		Length of the string. */
size_t strlen(const char *str) {
	size_t retval;
	for(retval = 0; *str != '\0'; str++) retval++;
	return retval;
}

/** Fill a memory area.
 * @param dest		The memory area to fill.
 * @param val		The value to fill with.
 * @param count		The number of bytes to fill.
 * @return		Destination address. */
void *memset(void *dest, int val, size_t count) {
	char *temp = (char *)dest;
	for(; count != 0; count--) *temp++ = val;
	return dest;
}

/** Copy data in memory.
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 * @return		Destination address. */
void *memcpy(void *dest, const void *src, size_t count) {
	char *d = (char *)dest;
	const char *s = (const char *)src;
	size_t i;

	for(i = 0; i < count; i++) {
		*d++ = *s++;
	}

	return dest;
}

/** Find a character in a string.
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * @return		NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
	char ch = c;

	for (;;) {
		if(*s == ch)
			break;
		else if(!*s)
			return NULL;
		else
			s++;
	}

	return (char *)s;
}

/** Copy a string.
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * @return		The value specified for dest. */
char *strcpy(char *dest, const char *src) {
	char *d = dest;

	while((*d++ = *src++));
	return dest;
}

/** Copy a string with a length limit.
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * @param count		Maximum number of bytes to copy.
 * @return		The value specified for dest. */
char *strncpy(char *dest, const char *src, size_t count) {
	size_t i;
	
	for(i = 0; i < count; i++) {
		dest[i] = src[i];
		if(!src[i]) {
			break;
		}
	}
	return dest;
}

/** Concatenate 2 strings.
 * @param dest		Pointer to the string to append to.
 * @param src		Pointer to the string to append.
 * @return		Pointer to dest. */
char *strcat(char *dest, const char *src) {
	size_t destlen = strlen(dest);
	char *d = dest + destlen;

	while((*d++ = *src++));
	return dest;
}

/** Compare 2 strings.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
	char x;

	for(;;) {
		x = *s1;
		if(x != *s2)
			break;
		if(!x)
			break;
		s1++;
		s2++;
	}
	return x - *s2;
}

/** Compare 2 strings with a length limit.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1;
	const char *b = s2;
	const char *fini = a + count;

	while(a < fini) {
		int res = *a - *b;
		if(res)
			return res;
		if(!*a)
			return 0;
		a++; b++;
	}
	return 0;
}

/** Duplicate a string.
 * @param s		Pointer to the source buffer.
 * @return		Pointer to the allocated buffer containing the string,
 *			or NULL on failure. */
char *strdup(const char *s) {
	char *dup;
	size_t len = strlen(s) + 1;

	dup = malloc(len);
	if(dup == NULL) {
		return NULL;
	}

	memcpy(dup, s, len);
	return dup;
}

/** Separate a string.
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
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if((s = *stringp) == NULL)
		return (NULL);

	for(tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;

				*stringp = s;
				return (tok);
			}
		} while(sc != 0);
	}
}
