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
 * @brief		Fixed heap allocator.
 */

#include <lib/fixed_heap.h>
#include <lib/utility.h>

#include <assert.h>
#include <kernel.h>

/** Fixed heap tag structure. */
typedef struct fixed_heap_tag {
	struct fixed_heap_tag *next;	/**< Next tag. */
	size_t data;			/**< Size and whether the tag is allocated. */
} fixed_heap_tag_t;

/** Check whether a tag is allocated.
 * @param tag		Tag to check.
 * @return		Whether the tag is allocated. */
static inline bool tag_allocated(fixed_heap_tag_t *tag) {
	return (tag->data & (1<<0));
}

/** Get the size of a tag.
 * @param tag		Tag to get size of.
 * @return		Size of the tag. */
static inline size_t tag_size(fixed_heap_tag_t *tag) {
	return (tag->data & ~(1<<0));
}

/** Allocate from a fixed heap.
 * @param heap		Heap to allocate from.
 * @param size		Size of allocation to make.
 * @return		Pointer to allocation, or NULL if no space left. */
void *fixed_heap_alloc(fixed_heap_t *heap, size_t size) {
	fixed_heap_tag_t *tag, *other;
	size_t total;

	if(!size) {
		return NULL;
	}

	/* Minimum size and alignment of 8 bytes. */
	total = ROUND_UP(size, 8) + sizeof(fixed_heap_tag_t);

	/* Search for a free segment. */
	for(tag = heap->tags; tag; tag = tag->next) {
		if(tag_allocated(tag) || tag_size(tag) < total) {
			continue;
		}

		/* Found a suitable segment, chop it up if necessary (and if
		 * there's enough room to do so). */
		if(tag_size(tag) > total && (tag_size(tag) - total) > (sizeof(fixed_heap_tag_t) + 8)) {
			other = (fixed_heap_tag_t *)((ptr_t)tag + total);
			other->next = tag->next;
			other->data = tag_size(tag) - total;
			tag->next = other;
			tag->data = total;
		}

		/* Mark as allocated. */
		tag->data |= (1<<0);
		return (void *)((ptr_t)tag + sizeof(fixed_heap_tag_t));
	}

	return NULL;
}

/** Free an allocation from a fixed heap.
 * @param heap		Heap to free to.
 * @param ptr		Allocation to free. */
void fixed_heap_free(fixed_heap_t *heap, void *ptr) {
	fixed_heap_tag_t *tag, *prev;

	if(!ptr) {
		return;
	}

	tag = (fixed_heap_tag_t *)((ptr_t)ptr - sizeof(fixed_heap_tag_t));

	if(unlikely(!tag_allocated(tag))) {
		fatal("Freeing already free segment %p (tag: %p)", ptr, tag);
	}

	/* Mark as free. */
	tag->data &= ~(1<<0);

	/* Check if there is a free tag after that we can merge with. */
	if(tag->next && !tag_allocated(tag->next)) {
		tag->data += tag->next->data;
		tag->next = tag->next->next;
	}

	/* Find the previous tag and see if we can merge with it. */
	if(tag != heap->tags) {
		for(prev = heap->tags; prev; prev = prev->next) {
			if(prev->next != tag) {
				continue;
			}

			if(!tag_allocated(prev)) {
				prev->data += tag->data;
				prev->next = tag->next;
			}

			return;
		}

		/* If we haven't seen a tag where next = the tag being freed,
		 * this is an invalid free. */
		fatal("Allocation %p does not come from heap %p", ptr, heap);
	}
}

/** Initialize a fixed heap allocator.
 * @param heap		Heap to initialize.
 * @param mem		Memory area to use.
 * @param size		Size of memory area. */
void fixed_heap_init(fixed_heap_t *heap, void *mem, size_t size) {
	assert(size >= (sizeof(fixed_heap_tag_t) + 8));
	assert(IS_POW2(size));

	/* Create an initial free segment covering the entire chunk. */
	heap->tags = mem;
	heap->tags->next = NULL;
	heap->tags->data = size;
}
