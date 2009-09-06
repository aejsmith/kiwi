/* strcasecmp/strncasecmp functions
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
 * @brief		Case-insensitive string comparison functions.
 */

#include <ctype.h>
#include <string.h>

/** Compare 2 strings ignorning case.
 *
 * Compares the two strings specified, ignorning the case of the characters
 * in the strings.
 *
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * 
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2.
 */
int strcasecmp(const char *s1, const char *s2) {
	for(;;) {
		if(!*s2 || (tolower(*s1) != tolower(*s2)))
			break;
		s1++;
		s2++;
	}
	return tolower(*s1) - tolower(*s2);
}

/** Compare 2 strings with a length limit ignoring case.
 *
 * Compares the two strings specified, ignorning the case of the characters
 * in the strings. Compares at most the number of bytes specified.
 *
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * 
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2.
 */
int strncasecmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1;
	const char *b = s2;
	const char *fini = a + count;

	while(a < fini) {
		int res = tolower(*a) - tolower(*b);
		if(res)
			return res;
		if(!*a)
			return 0;
		a++; b++;
	}
	return 0;
}
