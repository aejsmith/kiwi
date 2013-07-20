/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kernel library support functions.
 */

#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/object.h>

#include <stdarg.h>
#include <string.h>

#include "../libkernel.h"

/** Output handle to use (stderr). */
#define OUTPUT_HANDLE		2

/** Whether debug output is enabled. */
bool libkernel_debug = false;

/** Print a character.
 * @param ch		Character to print. */
static inline void printf_print_char(char ch) {
	kern_file_write(OUTPUT_HANDLE, &ch, 1, -1, NULL);
}

/** Print a string.
 * @param str		String to print. */
static inline void printf_print_string(const char *str) {
	kern_file_write(OUTPUT_HANDLE, str, strlen(str), -1, NULL);
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
			printf_print_char(format[0]);
			break;
		case 1:
			/* Handle literal %. */
			if(*format == '%') {
				printf_print_char('%');
				state = 0;
				break;
			}

			/* Handle conversion characters. */
			switch(*format) {
			case 's':
				str = va_arg(args, const char *);
				printf_print_string(str);
				break;
			case 'c':
				dec = va_arg(args, int);
				printf_print_char(dec);
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
			case 'z':
				continue;
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
