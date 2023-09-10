/*
 * Copyright (C) 2009-2023 Alex Smith
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
