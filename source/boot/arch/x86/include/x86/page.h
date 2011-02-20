/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		x86 paging definitions.
 */

#ifndef __X86_PAGE_H
#define __X86_PAGE_H

/** Definitions of paging structure bits. */
#define PG_PRESENT		(1<<0)		/**< Page is present. */
#define PG_WRITE		(1<<1)		/**< Page is writable. */
#define PG_USER			(1<<2)		/**< Page is accessible in CPL3. */
#define PG_PWT			(1<<3)		/**< Page has write-through caching. */
#define PG_PCD			(1<<4)		/**< Page has caching disabled. */
#define PG_ACCESSED		(1<<5)		/**< Page has been accessed. */
#define PG_DIRTY		(1<<6)		/**< Page has been written to. */
#define PG_LARGE		(1<<7)		/**< Page is a large page. */
#define PG_GLOBAL		(1<<8)		/**< Page won't be cleared in TLB. */

#endif /* __X86_PAGE_H */
