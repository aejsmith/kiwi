/*
 * Copyright (C) 2009-2012 Alex Smith
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
 * @brief               Memory management core definitions.
 */

#ifndef __MM_MM_H
#define __MM_MM_H

/** Allocation flags supported by all allocators. */
#define MM_WAIT			(1<<0)	/**< Block until memory is available, guarantees success. */
#define MM_BOOT			(1<<1)	/**< Allocation is required for boot. */
#define MM_ZERO			(1<<2)	/**< Zero out allocated memory. */

/** Mask to select only generic allocation flags.
 * @note		Does not include MM_ZERO - should be handled manually
 *			by each allocator as required. */
#define MM_FLAG_MASK		(MM_WAIT | MM_BOOT)

#endif /* __MM_MM_H */
