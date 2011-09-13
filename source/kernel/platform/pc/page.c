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
 * @brief		PC physical memory management.
 */

#include <mm/page.h>

#include <kboot.h>
#include <kernel.h>

#define A4G		0x100000000LL
#define A16M		0x1000000LL

/** Add memory ranges to the physical memory manager. */
__init_text void platform_page_init(void) {
	KBOOT_ITERATE(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
		switch(range->type) {
		case KBOOT_MEMORY_FREE:
		case KBOOT_MEMORY_ALLOCATED:
		case KBOOT_MEMORY_RECLAIMABLE:
			break;
		default:
			continue;
		}

		/* Determine which free list pages in the range should be put
		 * in. If necessary, split into multiple ranges. */
		if(range->start < A16M) {
			if(range->end <= A16M) {
				page_add_physical_range(range->start, range->end,
				                        PAGE_FREE_LIST_BELOW16M);
			} else if(range->end <= A4G) {
				page_add_physical_range(range->start, A16M,
				                        PAGE_FREE_LIST_BELOW16M);
				page_add_physical_range(A16M, range->end,
				                        PAGE_FREE_LIST_BELOW4G);
			} else {
				page_add_physical_range(range->start, A16M,
				                        PAGE_FREE_LIST_BELOW16M);
				page_add_physical_range(A16M, A4G,
				                        PAGE_FREE_LIST_BELOW4G);
				page_add_physical_range(A4G, range->end,
				                        PAGE_FREE_LIST_ABOVE4G);
			}
		} else if(range->start < A4G) {
			if(range->end <= A4G) {
				page_add_physical_range(range->start, range->end,
				                        PAGE_FREE_LIST_BELOW4G);
			} else {
				page_add_physical_range(range->start, A4G,
				                        PAGE_FREE_LIST_BELOW4G);
				page_add_physical_range(A4G, range->end,
				                        PAGE_FREE_LIST_ABOVE4G);
			}
		} else {
			page_add_physical_range(range->start, range->end, PAGE_FREE_LIST_ABOVE4G);
		}
	}
}
