/* Kiwi C library - strlen function
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
 * @brief		String length functions.
 */

#include <string.h>

/** Get length of string.
 *
 * Gets the length of the string specified. The length is the number of
 * characters found before a NULL byte.
 *
 * @param str		Pointer to the string.
 * 
 * @return		Length of the string.
 */
size_t strlen(const char *str) {
	size_t retval;
	for(retval = 0; *str != '\0'; str++) retval++;
	return retval;
}

/** Get length of string with limit.
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
	size_t retval;
	for(retval = 0; *str != '\0' && retval < count; str++) retval++;
	return retval;
}
