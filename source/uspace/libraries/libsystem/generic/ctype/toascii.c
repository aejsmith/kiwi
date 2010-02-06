/*
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
 * @brief		Character type functions.
 */

#include <ctype.h>

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
int toascii(int ch) {
	return (ch & 0x7F);
}
