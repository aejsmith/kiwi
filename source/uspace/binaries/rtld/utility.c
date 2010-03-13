/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		RTLD utility functions.
 */

#include <kernel/vm.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "args.h"
#include "utility.h"

extern int putch(char ch);

/** Size of the statically allocated heap. */
#define RTLD_HEAP_SIZE		16384

static uint8_t rtld_heap[RTLD_HEAP_SIZE];
static size_t rtld_heap_current = 0;

/** Map an object into memory.
 * @param start		Start address of region (if VM_MAP_FIXED).
 * @param size		Size of region to map.
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param handle	Handle to object to map in. If -1, then the region
 *			will be an anonymous memory mapping.
 * @param offset	Offset into object to map from.
 * @param addrp		Where to store address of mapping.
 * @return		0 on success, negative error code on failure. */
int vm_map(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp) {
	vm_map_args_t args = {
		.start = start,
		.size = size,
		.flags = flags,
		.handle = handle,
		.offset = offset,
		.addrp = addrp,
	};
	return _vm_map(&args);
}

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

/** Print a string.
 * @param str		String to print. */
static inline void printf_print_string(const char *str) {
	size_t i, len = strlen(str);

	for(i = 0; i < len; i++) {
		putch(str[i]);
	}
}

/** Print a hexadecimal value.
 * @param val		Value to print. */
static inline void printf_print_base16(unsigned long val) {
	char buf[20], *pos = buf + sizeof(buf);

	*--pos = 0;
	do {
		if((val & 0xF) <= 0x09) {
			*--pos = '0' + (val & 0xF);
		} else {
			*--pos = 'a' + ((val & 0xF) - 0xA);
		}
		val >>= 4;
	} while(val > 0);

	*--pos = 'x';
	*--pos = '0';
	printf_print_string(pos);
}

/** Print a decimal value.
 * @param val		Value to print. */
static inline void printf_print_base10(unsigned long val) {
	char buf[20], *pos = buf + sizeof(buf);

	*--pos = 0;
	do {
		*--pos = '0' + (val % 10); \
		val /= 10;
	} while(val > 0);

	printf_print_string(pos);
}

/** Quick and dirty printf()-style function.
 * @param format	Format string.
 * @param ...		Format arguments. */
static void do_printf(const char *format, va_list args) {
	int state = 0, dec;
	unsigned int udec;
	const char *str;
	void *ptr;

	for(; *format; format++) {
		switch(state) {
		case 0:
			if(*format == '%') {
				state = 1;
				break;
			}
			putch(format[0]);
			break;
		case 1:
			/* Handle literal %. */
			if(*format == '%') {
				putch('%');
				state = 0;
				break;
			}

			/* Handle conversion characters. */
			switch(*format) {
			case 's':
				str = va_arg(args, const char *);
				printf_print_string(str);
				break;
			case 'd':
				dec = va_arg(args, int);
				if(dec < 0) {
					printf_print_string("-");
					dec = -dec;
				}
				printf_print_base10((unsigned long)dec);
				break;
			case 'u':
				udec = va_arg(args, unsigned int);
				printf_print_base10((unsigned long)udec);
				break;
			case 'x':
				udec = va_arg(args, unsigned int);
				printf_print_base16((unsigned long)udec);
				break;
			case 'p':
				ptr = va_arg(args, void *);
				printf_print_base16((unsigned long)ptr);
				break;
			}
			state = 0;
			break;
		}
	}
}

/** Quick and dirty printf()-style function.
 * @note		Does not return the right value.
 * @param format	Format string.
 * @param ...		Format arguments. */
int printf(const char *format, ...) {
	va_list args;

	va_start(args, format);
	do_printf(format, args);
	va_end(args);

	return 0;
}

/** Quick and dirty printf()-style function for debug messages.
 * @param format	Format string.
 * @param ...		Format arguments. */
void dprintf(const char *format, ...) {
	va_list args;

	va_start(args, format);
	if(rtld_debug) {
		do_printf(format, args);
	}
	va_end(args);
}

/** Allocate some memory.
 * @param size		Size to allocate.
 * @return		Pointer to allocation on success, NULL on failure. */
void *malloc(size_t size) {
	void *ret;

	if((rtld_heap_current + size) > RTLD_HEAP_SIZE) {
		return NULL;
	}

	ret = &rtld_heap[rtld_heap_current];
	rtld_heap_current += size;
	return ret;
}

/** Free memory previously allocated with malloc().
 * @param addr		Address allocated. */
void free(void *addr) {
	/* Nothing happens. It probably should sometime. TODO. */
}
