/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 paging definitions.
 */

#pragma once

/** Page size definitions. */
#define PAGE_WIDTH          12          /**< Width of a page in bits. */
#define PAGE_SIZE           0x1000      /**< Size of a page (4KB). */
#define LARGE_PAGE_WIDTH    21          /**< Width of a large page in bits. */
#define LARGE_PAGE_SIZE     0x200000    /**< Size of a large page (2MB). */

/** Mask to clear page offset and unsupported bits from a virtual address. */
#define PAGE_MASK           0x000ffffffffff000l

/** Mask to clear page offset and unsupported bits from a physical address. */
#define PHYS_PAGE_MASK      0x000000fffffff000l

/** Number of free page lists. */
#define PAGE_FREE_LIST_COUNT        3

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
#define PAGE_FREE_LIST_ABOVE4G      0
#define PAGE_FREE_LIST_BELOW4G      1
#define PAGE_FREE_LIST_BELOW16M     2
