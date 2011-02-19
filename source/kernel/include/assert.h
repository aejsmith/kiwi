/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Assertion function.
 */

#ifndef __ASSERT_H
#define __ASSERT_H

#include <compiler.h>
#ifdef LOADER
# include <boot/error.h>
#else
# include <kernel.h>
#endif

#if CONFIG_DEBUG
# ifdef LOADER
#  define assert(cond)	if(unlikely(!(cond))) { internal_error("Assertion failure: %s\nat %s:%d", #cond, __FILE__, __LINE__); }
# else
#  define assert(cond)	if(unlikely(!(cond))) { fatal("Assertion failure: %s\nat %s:%d", #cond, __FILE__, __LINE__); }
# endif
#else
# define assert(cond)	((void)0)
#endif

#endif /* __ASSERT_H */
