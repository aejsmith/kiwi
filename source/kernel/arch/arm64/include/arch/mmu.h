/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 MMU context definitions.
 */

#pragma once

#include <types.h>

/** Size of TLB invalidation queue. */
#define ARCH_MMU_INVALIDATE_QUEUE_SIZE  128

/** ARM64 MMU context structure. */
typedef struct arch_mmu_context {
    uint16_t asid;                  /**< Address space ID. */
    phys_ptr_t ttl0;                /**< Physical address of level 0 translation table. */

    /** Queue of TLB entries to flush when unlocking the context. */
    ptr_t invalidate_queue[ARCH_MMU_INVALIDATE_QUEUE_SIZE];
    size_t invalidate_count;
} arch_mmu_context_t;
