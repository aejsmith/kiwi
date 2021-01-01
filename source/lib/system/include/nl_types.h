/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Native language support data types.
 */

#pragma once

#include <locale.h>
#include <stdarg.h>
#define __need_wint_t
#include <stddef.h>
#include <stdio.h>

__SYS_EXTERN_C_BEGIN

/** Type used to identify a message catalog descriptor. */
typedef void *nl_catd;

/** Type used by nl_langinfo() to identify items of langinfo data. */
typedef int nl_item;

/* Default message set. */
#define NL_SETD             1

/** Flag for catopen() to use LC_MESSAGES rather than LANG in the environment. */
#define NL_CAT_LOCALE       (1<<0)

// Needed for libcxx build.
#ifdef __cplusplus

extern int catclose(nl_catd);
extern char *catgets(nl_catd, int, int, const char *);
extern nl_catd catopen(const char *, int);

#endif

__SYS_EXTERN_C_END
