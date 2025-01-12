/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Native language support data types.
 */

#pragma once

#define __NEED_wint_t
#include <bits/alltypes.h>

#include <locale.h>
#include <stdarg.h>
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
