/*
 * Copyright (C) 2008-2009 Alex Smith
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
