/*
 * Copyright (C) 2010 Alex Smith
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

/** Print a character.
 * @param ch		Character to print. */
static inline void printf_print_char(char ch) {
	switch(object_type(OUTPUT_HANDLE)) {
	case OBJECT_TYPE_DEVICE:
		device_write(OUTPUT_HANDLE, &ch, 1, 0, NULL);
		break;
	case OBJECT_TYPE_FILE:
		fs_file_write(OUTPUT_HANDLE, &ch, 1, NULL);
		break;
	}
}

/** Print a string.
 * @param str		String to print. */
static inline void printf_print_string(const char *str) {
	switch(object_type(OUTPUT_HANDLE)) {
	case OBJECT_TYPE_DEVICE:
		device_write(OUTPUT_HANDLE, str, strlen(str), 0, NULL);
		break;
	case OBJECT_TYPE_FILE:
		fs_file_write(OUTPUT_HANDLE, str, strlen(str), NULL);
		break;
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
