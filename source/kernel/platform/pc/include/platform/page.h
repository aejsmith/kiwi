/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		PC physical memory management definitions.
 */

#ifndef __PLATFORM_PAGE_H
#define __PLATFORM_PAGE_H

/** Number of free page lists. */
#define PAGE_FREE_LIST_COUNT		3

/**
 * Free page list number definitions.
 *
 * On the PC, we split into 3 lists: below 16MB (for ISA DMA), below 4GB (for
 * devices needing 32-bit DMA addresses) and the rest. Since the page allocator
 * will search the lists from lowest index to highest, we place over 4GB first,
 * then below 4GB, then 16MB. This means that wherever possible allocations
 * will be made from higher regions, making allocations from the lower regions
 * when they are actually required more likely to succeed.
 */
#define PAGE_FREE_LIST_ABOVE4G		0
#define PAGE_FREE_LIST_BELOW4G		1
#define PAGE_FREE_LIST_BELOW16M		2

#endif /* __PLATFORM_PAGE_H */
