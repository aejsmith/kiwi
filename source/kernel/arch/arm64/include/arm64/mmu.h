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
 * @brief               ARM64 MMU definitions.
 */

#pragma once

#include <types.h>

/** Definitions of paging structure bits. */
#define ARM64_TTE_PRESENT               (1ul<<0)    /**< Entry is present. */
#define ARM64_TTE_TABLE                 (1ul<<1)    /**< Entry is a table. */
#define ARM64_TTE_PAGE                  (1ul<<1)    /**< Entry is a page. */
#define ARM64_TTE_AP_P_RW_U_NA          (0ul<<6)    /**< Protected RW, user not accessible. */
#define ARM64_TTE_AP_P_RW_U_RW          (1ul<<6)    /**< Protected RW, user RW. */
#define ARM64_TTE_AP_P_RO_U_NA          (2ul<<6)    /**< Protected RO, user not accessible. */
#define ARM64_TTE_AP_P_RO_U_RO          (3ul<<6)    /**< Protected RO, user RO. */
#define ARM64_TTE_AP_MASK               (3ul<<6)
#define ARM64_TTE_SH_NON_SHAREABLE      (0ul<<8)
#define ARM64_TTE_SH_OUTER_SHAREABLE    (2ul<<8)
#define ARM64_TTE_SH_INNER_SHAREABLE    (3ul<<8)
#define ARM64_TTE_SH_MASK               (3ul<<8)
#define ARM64_TTE_AF                    (1ul<<10)   /**< Entry has been accessed. */
#define ARM64_TTE_NG                    (1ul<<11)   /**< Entry is not global. */
#define ARM64_TTE_XN                    (1ul<<54)   /**< Entry disallows execute. */
#define ARM64_TTE_ATTR_INDEX(value)     (((unsigned long)(value))<<2)
#define ARM64_TTE_ATTR_INDEX_MASK       0x000000000000001cul

/** Masks to get physical address from a page table entry. */
#define ARM64_TTE_ADDR_MASK             0x00007ffffffff000ul

/** Ranges covered by paging structures. */
#define ARM64_TTL1_RANGE                0x8000000000ul
#define ARM64_TTL2_RANGE                0x40000000
#define ARM64_TTL3_RANGE                0x200000

/**
 * MAIR attribute indices corresponding to MMU_CACHE_* types. Note these line
 * up with KBoot's indices, though it shouldn't matter too much as there's only
 * a short window between setting MAIR and swapping over to the kernel MMU
 * context.
 */
#define ARM64_MAIR_INDEX_NORMAL         0   /**< Normal, Write-Back, Read-/Write-Allocate. */
#define ARM64_MAIR_INDEX_WRITE_COMBINE  1   /**< Normal, Non-cacheable. */
#define ARM64_MAIR_INDEX_UNCACHED       2   /**< Device-nGnRnE. */
#define ARM64_MAIR_INDEX_DEVICE         3   /**< Device-nGnRE. */

/** MAIR value corresponding to the above indices. */
#define ARM64_MAIR_ENTRY(idx, val)      ((uint64_t)(val) << ((idx) * 8))
#define ARM64_MAIR ( \
    ARM64_MAIR_ENTRY(0, 0b11111111) | \
    ARM64_MAIR_ENTRY(1, 0b01000100) | \
    ARM64_MAIR_ENTRY(2, 0b00000000) | \
    ARM64_MAIR_ENTRY(3, 0b00000100))
