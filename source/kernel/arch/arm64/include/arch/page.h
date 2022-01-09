/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               ARM64 paging definitions.
 */

#pragma once

/** Page size definitions. */
#define PAGE_WIDTH          12          /**< Width of a page in bits. */
#define PAGE_SIZE           0x1000      /**< Size of a page (4KB). */
#define LARGE_PAGE_WIDTH    21          /**< Width of a large page in bits. */
#define LARGE_PAGE_SIZE     0x200000    /**< Size of a large page (2MB). */

/** Mask to clear page offset and unsupported bits from a virtual address. */
#define PAGE_MASK           0x0000fffffffff000ul

/** Mask to clear page offset and unsupported bits from a physical address. */
#define PHYS_PAGE_MASK      0x00007ffffffff000ul

/** Number of free page lists. */
#define PAGE_FREE_LIST_COUNT        2

/**
 * Free page list number definitions.
 *
 * Split into 2 lists: below 4GB (for devices needing 32-bit DMA addresses),
 * and the rest. Since the page allocator will search the lists from lowest
 * index to highest, we place over 4GB first to prefer that where it can be
 * supported, keeping below 4GB for where it is needed.
 */
#define PAGE_FREE_LIST_ABOVE4G      0
#define PAGE_FREE_LIST_BELOW4G      1
