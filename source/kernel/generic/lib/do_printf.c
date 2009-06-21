/* Kiwi formatted output function
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
 * @brief		Formatted output function.
 */

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/do_printf.h>

#include <types.h>

/* Flags to specify special behaviour. */
#define PRINTF_ZERO_PAD		(1<<0)	/**< Pad with zeros. */
#define PRINTF_SIGN_CHAR	(1<<1)	/**< Always print a sign character for a signed conversion. */
#define PRINTF_SPACE_CHAR	(1<<2)	/**< Print a space before a positive result of a signed conversion. */
#define PRINTF_LEFT_JUSTIFY	(1<<3)	/**< Print left-justified. */
#define PRINTF_PREFIX		(1<<4)	/**< Print a 0x prefix for base 16 values. */
#define PRINTF_LOW_CASE		(1<<5)	/**< Print hexadecimal characters in lower case. */
#define PRINTF_SIGNED		(1<<6)	/**< Treat number as a signed value. */

/** Digits to use for printing numbers. */
static const char printf_digits_upper[] = "0123456789ABCDEF";
static const char printf_digits_lower[] = "0123456789abcdef";

/** Helper to print a string of characters.
 * @param str		String to write.
 * @param len		Length to print.
 * @param helper	Helper function.
 * @param data		Data for helper function.
 * @param total		Pointer to integer containing total characters printed. */
static void printf_string_helper(const char *str, size_t len, printf_helper_t helper, void *data, int *total) {
	size_t i;

	for(i = 0; i < len; i++) {
		helper(str[i], data, total);
	}
}

/** Helper to print a number.
 * @param num		Number to print.
 * @param width		Field width.
 * @param precision	Precision.
 * @param base		Number base to use.
 * @param flags		Internal behaviour flags.
 * @param helper	Helper function.
 * @param data		Data for helper function.
 * @param total		Pointer to integer containing total characters printed. */
static void printf_number_helper(uint64_t num, long width, long precision, int base, int flags,
                                 printf_helper_t helper, void *data, int *total) {
	char buffer[64];
	char sign = 0;
	uint32_t tmp;
	int i = 0;

	/* Work out the sign character to use, if any. Always print a sign
	 * character if the number is negative. */
	if(flags & PRINTF_SIGNED) {
		if((int64_t)num < 0) {
			sign = '-';
			num = -((int64_t)num);
			width--;
		} else if(flags & PRINTF_SIGN_CHAR) {
			sign = '+';
			width--;
		} else if(flags & PRINTF_SPACE_CHAR) {
			sign = ' ';
			width--;
		}
	}

	/* Reduce field width to accomodate any prefixes required. */
	if(flags & PRINTF_PREFIX) {
		if(base == 8) {
			width -= 1;
		} else if(base == 16) {
			width -= 2;
		}
	}

	/* Write the number out to the temporary buffer, in reverse order. */
	while(num != 0) {
		tmp = (uint32_t)(num % base);
		num /= base;
		buffer[i++] = (flags & PRINTF_LOW_CASE) ? printf_digits_lower[tmp] : printf_digits_upper[tmp];
	}

	/* Modify precision to store the number of actual digits we are going
	 * to print. The precision is the minimum number of digits to print,
	 * so if the digit count is higher than the precision, set precision
	 * to the digit count. Width then becomes the amount of padding we
	 * require. */
	if(i > precision) {
		precision = i;
	}
	width -= precision;

	/* If we're not left aligned and require space padding, write it.
	 * Do not handle zero padding here, sign and prefix characters must
	 * be before zero padding but after space padding. */
	if(!(flags & (PRINTF_LEFT_JUSTIFY | PRINTF_ZERO_PAD))) {
		while(--width >= 0) {
			helper(' ', data, total);
		}
	}

	/* Write out the sign character, if any. */
	if(sign) {
		helper(sign, data, total);
	}

	/* Write out any prefix required. Base 8 has a '0' prefix, base 16
	 * has a '0x' or '0X' prefix, depending on whether lower or upper
	 * case. */
	if(flags & PRINTF_PREFIX && (base == 8 || base == 16)) {
		helper('0', data, total);
		if(base == 16) {
			helper((flags & PRINTF_LOW_CASE) ? 'x' : 'X', data, total);
		}
	}

	/* Do zero padding. */
	if(!(flags & PRINTF_LEFT_JUSTIFY) && flags & PRINTF_ZERO_PAD) {
		while(--width >= 0) {
			helper('0', data, total);
		}
	}
	while(i <= --precision) {
		helper('0', data, total);
	}

	/* Write out actual digits, reversed to the correct direction. */
	while(--i >= 0) {
		helper(buffer[i], data, total);
	}

	/* Finally handle space padding caused by left justification. */
	if(flags & (PRINTF_LEFT_JUSTIFY)) {
		while(--width >= 0) {
			helper(' ', data, total);
		}
	}
}

/** Internal implementation of printf()-style functions.
 *
 * This function does the main work of printf()-style functions. It parses
 * the format string, and uses a supplied helper function to actually put
 * characters in the desired output location (for example the console or a
 * string buffer).
 *
 * @note		Floating point values are not supported.
 * @note		The 'n' conversion specifier is not supported.
 * @note		The 't' length modifier is not supported.
 *
 * @param helper	Helper function to use.
 * @param data		Data to pass to helper function.
 * @param fmt		Format string.
 * @param args		List of arguments.
 *
 * @return		Number of characters written.
 */
int do_printf(printf_helper_t helper, void *data, const char *fmt, va_list args) {
	int total = 0, flags, base;
	long width, precision;
	unsigned char ch;
	const char *str;
	uint64_t num;
	int32_t len;

	for(; *fmt; fmt++) {
		if(*fmt != '%') {
			helper(*fmt, data, &total);
			continue;
		}

		/* Parse flags in the format string. */
		flags = 0;
		while(true) {
			switch(*(++fmt)) {
			case '#':
				flags |= PRINTF_PREFIX;
				continue;
			case '0':
				/* Left justify has greater priority than
				 * zero padding. */
				if(!(flags & PRINTF_LEFT_JUSTIFY)) {
					flags |= PRINTF_ZERO_PAD;
				}
				continue;
			case '-':
				flags &= ~PRINTF_ZERO_PAD;
				flags |= PRINTF_LEFT_JUSTIFY;
				continue;
			case ' ':
				flags |= PRINTF_SPACE_CHAR;
				continue;
			case '+':
				flags |= PRINTF_SIGN_CHAR;
				continue;
			}
			break;
		}

		/* Work out the field width. If there is a digit at the
		 * current position in the string, then the field width is
		 * in the string. If there is a '*' character, the field
		 * width is the next argument (a negative field width argument
		 * implies left justification, and the width will be made
		 * positive). */
		if(isdigit(*fmt)) {
			width = strtol(fmt, (char **)&fmt, 10);
		} else if(*fmt == '*') {
			width = (long)va_arg(args, int);
			if(width < 0) {
				flags |= PRINTF_LEFT_JUSTIFY;
				width = -width;
			}
			fmt++;
		} else {
			width = -1;
		}

		/* If there is a period character in the string, there is a
		 * precision. This can also be specified as a '*' character,
		 * like for field width, except a negative value means 0. */
		if(*fmt == '.') {
			fmt++;
			if(isdigit(*fmt)) {
				if((precision = strtol(fmt, (char **)&fmt, 10)) < 0) {
					precision = 0;
				}
			} else if(*fmt == '*') {
				if((precision = (long)va_arg(args, int)) < 0) {
					precision = 0;
				}
				fmt++;
			} else {
				precision = 0;
			}
		} else {
			precision = -1;
		}

		/* Get the length modifier. */
		switch(*fmt) {
		case 'h':
		case 'z':
			len = (uint32_t)*(fmt++);
			break;
		case 'l':
			len = (uint32_t)*(fmt++);
			if(*fmt == 'l') {
				len |= (1<<8);
				fmt++;
			}
			break;
		default:
			len = 0;
			break;
		}

		/* Get and handle the conversion specifier. For number
		 * conversions, we break out of the switch to get to the
		 * number handling code. For anything else, continue to the
		 * next iteration of the main loop. */
		base = 10;
		switch(*fmt) {
		case '%':
			helper('%', data, &total);
			continue;
		case 'c':
			ch = (unsigned char)va_arg(args, int);
			if(flags & PRINTF_LEFT_JUSTIFY) {
				helper(ch, data, &total);
				while(--width > 0) {
					helper(' ', data, &total);
				}
			} else {
				while(--width > 0) {
					helper(' ', data, &total);
				}
				helper(ch, data, &total);
			}
			continue;
		case 'd':
		case 'i':
			flags |= PRINTF_SIGNED;
			break;
		case 'o':
			base = 8;
			break;
		case 'p':
			/* Print lower-case and as though # was specified. */
			flags |= (PRINTF_LOW_CASE | PRINTF_PREFIX);
			if(precision == -1) {
				precision = 1;
			}

			/* Pointers should not go through number conversion. */
			printf_number_helper((ptr_t)va_arg(args, void *),
			                     width, precision, 16, flags,
			                     helper, data, &total);
			continue;
		case 's':
			/* We won't need the length modifier here, can use the
			 * len variable. */
			str = va_arg(args, const char *);
			len = strnlen(str, precision);
			if(flags & PRINTF_LEFT_JUSTIFY) {
				printf_string_helper(str, len, helper, data, &total);
				while(len < width--) {
					helper(' ', data, &total);
				}
			} else {
				while(len < width--) {
					helper(' ', data, &total);
				}
				printf_string_helper(str, len, helper, data, &total);
			}
			continue;
		case 'u':
			break;
		case 'x':
			flags |= PRINTF_LOW_CASE;
		case 'X':
			base = 16;
			break;
		default:
			/* Unknown character, go back and reprint what we
			 * skipped over. */
			helper('%', data, &total);
			while(fmt[-1] != '%') {
				fmt--;
			}
			continue;
		}

		/* Default precision for numbers should be 1. */
		if(precision == -1) {
			precision = 1;
		}

		/* Perform conversions according to the length modifiers. */
		switch(len & 0xff) {
		case 'h':
			num = (unsigned short)va_arg(args, int);
			if(flags & PRINTF_SIGNED) {
				num = (signed short)num;
			}
			break;			
		case 'l':
			/* Bit 8 is set to mean 'll' rather than 'l'. */
			if(len & (1<<8)) {
				num = va_arg(args, unsigned long long);
			} else {
				num = va_arg(args, unsigned long);
				if(flags & PRINTF_SIGNED) {
					num = (signed long)num;
				}
			}
			break;
		case 'z':
			num = va_arg(args, size_t);
			break;
		default:
			num = va_arg(args, unsigned int);
			if(flags & PRINTF_SIGNED) {
				num = (signed int)num;
			}
		}

		/* Print the number. */
		printf_number_helper(num, width, precision, base, flags, helper, data, &total);
	}

	return total;
}
