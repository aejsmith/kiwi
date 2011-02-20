/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Time handling functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <types.h>

/** Convert microseconds to seconds. */
#define USECS2SECS(secs)	(secs / 1000000)

/** Convert seconds to microseconds. */
#define SECS2USECS(secs)	((useconds_t)secs * 1000000)

/** Convert microseconds to milliseconds. */
#define USECS2MSECS(msecs)	(msecs / 1000)

/** Convert milliseconds to microseconds. */
#define MSECS2USECS(msecs)	((useconds_t)msecs * 1000)

extern void spin(timeout_t us);

#endif /* __TIME_H */
