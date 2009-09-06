/* Character type functions
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

#ifndef __CTYPE_H
#define __CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int isalnum(int ch);
extern int isalpha(int ch);
extern int isascii(int ch);
extern int isblank(int ch);
extern int iscntrl(int ch);
extern int isdigit(int ch);
extern int isgraph(int ch);
extern int islower(int ch);
extern int isprint(int ch);
extern int ispunct(int ch);
extern int isspace(int ch);
extern int isupper(int ch);
extern int isxdigit(int ch);
extern int toascii(int ch);
extern int tolower(int ch);
extern int toupper(int ch);

#define _tolower(ch)	((ch) | 0x20)
#define _toupper(ch)	((ch) & ~0x20)

#ifdef __cplusplus
}
#endif

#endif /* __CTYPE_H */
