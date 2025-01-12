/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 physical memory management.
 */

#include <mm/page.h>

#include <kboot.h>
#include <kernel.h>

#define A4G         0x100000000ll
#define A16M        0x1000000ll

/** Add memory ranges to the physical memory manager. */
__init_text void arch_page_init(void) {
    kboot_tag_foreach(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
        phys_ptr_t end = range->start + range->size;

        /* Determine which free list pages in the range should be put in. If
         * necessary, split into multiple ranges. */
        if (range->start < A16M) {
            if (end <= A16M) {
                page_add_memory_range(range->start, end, PAGE_FREE_LIST_BELOW16M);
            } else if (end <= A4G) {
                page_add_memory_range(range->start, A16M, PAGE_FREE_LIST_BELOW16M);
                page_add_memory_range(A16M, end, PAGE_FREE_LIST_BELOW4G);
            } else {
                page_add_memory_range(range->start, A16M, PAGE_FREE_LIST_BELOW16M);
                page_add_memory_range(A16M, A4G, PAGE_FREE_LIST_BELOW4G);
                page_add_memory_range(A4G, end, PAGE_FREE_LIST_ABOVE4G);
            }
        } else if (range->start < A4G) {
            if (end <= A4G) {
                page_add_memory_range(range->start, end, PAGE_FREE_LIST_BELOW4G);
            } else {
                page_add_memory_range(range->start, A4G, PAGE_FREE_LIST_BELOW4G);
                page_add_memory_range(A4G, end, PAGE_FREE_LIST_ABOVE4G);
            }
        } else {
            page_add_memory_range(range->start, end, PAGE_FREE_LIST_ABOVE4G);
        }
    }
}
