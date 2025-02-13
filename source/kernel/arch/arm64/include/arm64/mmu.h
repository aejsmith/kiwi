/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 MMU definitions.
 */

#pragma once

#include <arm64/cpu.h>

/** Definitions of paging structure bits. */
#define ARM64_TTE_PRESENT               (1ul<<0)    /**< Entry is present. */
#define ARM64_TTE_TABLE                 (1ul<<1)    /**< Entry is a table (TTL0-2). */
#define ARM64_TTE_PAGE                  (1ul<<1)    /**< Entry is a page (TTL3). */
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

#define ARM64_TTE_CACHE_NORMAL \
    (ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_NORMAL) |  ARM64_TTE_SH_INNER_SHAREABLE)
#define ARM64_TTE_CACHE_DEVICE \
    (ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_DEVICE) | ARM64_TTE_SH_OUTER_SHAREABLE)
#define ARM64_TTE_CACHE_UNCACHED \
    (ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_UNCACHED) | ARM64_TTE_SH_OUTER_SHAREABLE)
#define ARM64_TTE_CACHE_WRITE_COMBINE \
    (ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_WRITE_COMBINE) | ARM64_TTE_SH_OUTER_SHAREABLE)

/*
 * Common TCR configuration:
 *  - 48-bit virtual address.
 *  - 48-bit intermediate physical address.
 *  - Write-back/write-allocate, inner shareable translation tables.
 *  - 4KB granule.
 */
#define ARM64_TCR_COMMON \
    ((16 << ARM64_TCR_T0SZ_SHIFT) | \
     ARM64_TCR_IRGN0_WB_WA | \
     ARM64_TCR_ORGN0_WB_WA | \
     ARM64_TCR_SH0_INNER | \
     ARM64_TCR_TG0_4 | \
     (16 << ARM64_TCR_T1SZ_SHIFT) | \
     ARM64_TCR_IRGN1_WB_WA | \
     ARM64_TCR_ORGN1_WB_WA | \
     ARM64_TCR_SH1_INNER | \
     ARM64_TCR_TG1_4 | \
     ARM64_TCR_IPS_48 | \
     ARM64_TCR_TBI0 | \
     ARM64_TCR_TBI1)

/*
 * TCR value for the kernel.
 *  - TTBR0 disabled (EPD0 set).
 *  - TTBR1 defines ASID (A1 set).
 */
#define ARM64_TCR_KERNEL \
    (ARM64_TCR_COMMON | \
     ARM64_TCR_EPD0 | \
     ARM64_TCR_A1)

/*
 * TCR value for userspace.
 *  - TTBR0 enabled (EPD0 clear).
 *  - TTBR0 defines ASID (A1 clear).
 */
#define ARM64_TCR_USER \
    ARM64_TCR_COMMON

/** ASID definitions. */
#define ARM64_ASID_UNUSED               0
#define ARM64_ASID_KERNEL               1
#define ARM64_ASID_USER_START           2
#define ARM64_ASID_USER_COUNT           254
