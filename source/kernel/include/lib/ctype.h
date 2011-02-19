/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Character type functions.
 */

#ifndef __LIB_CTYPE_H
#define __LIB_CTYPE_H

/** Test if character is lower-case.
 *
 * Tests that the given character is a lower-case character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is lower-case, zero if not.
 */
static inline int islower(int ch) {
	return (ch >= 'a' && ch <= 'z');
}

/** Test if character is upper-case.
 *
 * Tests that the given character is an upper-case character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is upper-case, zero if not.
 */
static inline int isupper(int ch) {
	return (ch >= 'A' && ch <= 'Z');
}

/** Test if character is alphabetic.
 *
 * Tests that the given character is an alphabetic character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is alphabetic, zero if not.
 */
static inline int isalpha(int ch) {
	return islower(ch) || isupper(ch);
}

/** Test if character is a digit.
 *
 * Tests that the given character is a digit.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is digit, zero if not.
 */
static inline int isdigit(int ch) {
	return (ch >= '0' && ch <= '9');
}

/** Test if character is alpha-numeric.
 *
 * Tests that the given character is alpha-numeric.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is alpha-numeric, zero if not.
 */
static inline int isalnum(int ch) {
	return (isalpha(ch) || isdigit(ch));
}

/** Test if character is an ASCII character.
 *
 * Tests that the given character is an ASCII character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is ASCII, zero if not.
 */
static inline int isascii(int ch) {
	return ((unsigned int)ch < 128u);
}

/** Test if character is blank.
 *
 * Tests that the given character is a blank space.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is blank space, zero if not.
 */
static inline int isblank(int ch) {
	return (ch == ' ' || ch == '\t');
}

/** Test if character is control character.
 *
 * Tests that the given character is a control character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is control character, zero if not.
 */
static inline int iscntrl(int ch) {
	return ((unsigned int)ch < 32u || ch == 127);
}

/** Test if character is a printable character.
 *
 * Tests that the given character is a printable character.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is printable, zero if not.
 */
static inline int isprint(int ch) {
	ch &= 0x7F;
	return (ch >= 0x20 && ch < 0x7F);
}

/** Check for any printable character except space.
 *
 * Same as isprint() except doesn't include space.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if check passed, zero if not.
 */
static inline int isgraph(int ch) {
	if(ch == ' ') {
		return 0;
	}
	return isprint(ch);
}

/** Test if character is space.
 *
 * Tests that the given character is a form of whitespace.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is whitespace, zero if not.
 */
static inline int isspace(int ch) {
	return (ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ');
}

/** Test if character is punctuation.
 *
 * Tests that the given character is a form of punctuation.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is punctuation, zero if not.
 */
static inline int ispunct(int ch) {
	return (isprint(ch) && !isalnum(ch) && !isspace(ch));
}

/** Test if character is a hexadecimal digit.
 *
 * Tests that the given character is a hexadecimal digit.
 *
 * @param ch		Character to test.
 *
 * @return		Non-zero if is alphabetic, zero if not.
 */
static inline int isxdigit(int ch) {
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/** Convert a character to ASCII.
 *
 * Converts a character to a 7-bit value that fits into the ASCII character
 * set. Using this function will upset people, as it converts accented
 * characters into random characters.
 *
 * @param ch		Character to convert.
 *
 * @return		Converted value.
 */
static inline int toascii(int ch) {
	return (ch & 0x7F);
}

/** Convert character to lower-case.
 *
 * Converts the given character to lower case.
 *
 * @param ch		Character to convert.
 *
 * @return		Converted character.
 */
static inline int tolower(int ch) {
	if(isalpha(ch)) {
		return ch | 0x20;
	} else {
		return ch;
	}
}

/** Convert character to upper-case.
 *
 * Converts the given character to upper case.
 *
 * @param ch		Character to convert.
 *
 * @return		Converted character.
 */
static inline int toupper(int ch) {
	if(isalpha(ch)) {
		return ch & ~0x20;
	} else {
		return ch;
	}
}

#define _tolower(ch)	((ch) | 0x20)
#define _toupper(ch)	((ch) & ~0x20)

#endif /* __LIB_CTYPE_H */
