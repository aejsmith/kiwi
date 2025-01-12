/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 MMU context definitions.
 */

#pragma once

#include <types.h>

/** Size of TLB flush array. */
#define ARCH_MMU_INVALIDATE_QUEUE_SIZE  128

/** AMD64 MMU context structure. */
typedef struct arch_mmu_context {
    phys_ptr_t pml4;                /**< Physical address of the PML4. */

    /** Queue of TLB entries to flush when unlocking the context. */
    ptr_t invalidate_queue[ARCH_MMU_INVALIDATE_QUEUE_SIZE];
    size_t invalidate_count;
} arch_mmu_context_t;
