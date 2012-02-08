/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Memory allocation functions.
 *
 * Simple set of malloc()/free() style functions implemented on top of the
 * slab allocator. The use of specialized slab caches is preferred over these
 * functions, however these are still useful for allocating temporary storage
 * when copying from userspace, or when allocating string buffers, etc.
 *
 * Cache sizes go up in powers of two, starting from 32 with a limitation of
 * 64K. For 64-bit systems, the boundary tag structure is 16 bytes, so having
 * caches smaller than 32 bytes is pointless. Allocations use the smallest
 * cache that can fit both the allocation and its information structure. If an
 * allocation larger than 64K is requested, then the allocation will use the
 * kernel heap directly.
 *
 * Allocations are tracked using an alloc_tag_t structure, which is placed
 * before the allocation in memory. It tracks the size of the allocation and
 * the cache it came from. If the allocation came from the heap, then the
 * cache pointer will be NULL.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/heap.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <kernel.h>

/** Information structure prepended to allocations. */
typedef struct alloc_tag {
	size_t size;			/**< Size of the allocation. */
	slab_cache_t *cache;		/**< Pointer to cache for allocation. */
} alloc_tag_t;

/** Cache settings. */
#define KMALLOC_CACHE_MIN	5	/**< Minimum cache size (2^5  == 32). */
#define KMALLOC_CACHE_MAX	16	/**< Maximum cache size (2^16 == 64K). */

/** Slab caches for kmalloc() and friends. */
static slab_cache_t *kmalloc_caches[KMALLOC_CACHE_MAX - KMALLOC_CACHE_MIN + 1];

/** Allocate a block of memory.
 * @param size		Size of block.
 * @param kmflag	Allocation flags.
 * @return		Pointer to block on success, NULL on failure. */
void *kmalloc(size_t size, int kmflag) {
	size_t total = size + sizeof(alloc_tag_t), idx;
	alloc_tag_t *addr;

	/* Use the slab caches where possible. */
	if(total <= (1 << KMALLOC_CACHE_MAX)) {
		/* If exactly a power-of-two, then highbit(total) will work,
		 * else we want the next size up. Remember that the highbit
		 * function returns (log2(n) + 1). */
		idx = (IS_POW2(total)) ? highbit(total) - 1 : highbit(total);
		if(idx < KMALLOC_CACHE_MIN) {
			idx = KMALLOC_CACHE_MIN;
		}
		idx -= KMALLOC_CACHE_MIN;

		addr = slab_cache_alloc(kmalloc_caches[idx], kmflag);
		if(unlikely(!addr)) {
			return NULL;
		}

		addr->cache = kmalloc_caches[idx];
	} else {
		/* Fall back on the kernel heap. */
		addr = heap_alloc(ROUND_UP(total, PAGE_SIZE), kmflag & MM_FLAG_MASK);
		if(unlikely(!addr)) {
			return NULL;
		}

		addr->cache = NULL;
	}

	addr->size = size;
	return &addr[1];
}

/** Allocate an array of zeroed memory.
 * @param nmemb		Number of array elements.
 * @param size		Size of each element.
 * @return		Pointer to block on success, NULL on failure. */
void *kcalloc(size_t nmemb, size_t size, int kmflag) {
	void *ret;

	ret = kmalloc(nmemb * size, kmflag);
	if(ret == NULL) {
		return NULL;
	}

	memset(ret, 0, nmemb * size);
	return ret;
}

/**
 * Resizes an allocated memory block.
 *
 * Resizes a memory block previously allocated with kmalloc(), kcalloc() or
 * krealloc(). If passed a NULL pointer, call is equivalent to
 * kmalloc(size, kmflag).
 *
 * @param addr		Address to resize.
 * @param size		New size.
 *
 * @return		Pointer to block on success, NULL on failure.
 */
void *krealloc(void *addr, size_t size, int kmflag) {
	alloc_tag_t *tag;
	void *ret;

	if(!addr) {
		return kmalloc(size, kmflag);
	}

	tag = (alloc_tag_t *)((char *)addr - sizeof(alloc_tag_t));
	if(tag->size == size) {
		return addr;
	}

	/* Make a new allocation. */
	ret = kmalloc(size, kmflag);
	if(!ret) {
		return ret;
	}

	/* Copy the block data using the smallest of the two sizes. */
	memcpy(ret, addr, MIN(tag->size, size));

	/* Free the old allocation. */
	kfree(addr);
	return ret;
}

/**
 * Free a block of memory.
 *
 * Frees a block of memory previously allocated with kmalloc(), kcalloc() or
 * krealloc().
 *
 * @param addr		Address to free. If NULL, nothing is done.
 */
void kfree(void *addr) {
	alloc_tag_t *tag;

	if(addr) {
		tag = (alloc_tag_t *)((char *)addr - sizeof(alloc_tag_t));

		/* If the cache pointer is not set, assume the allocation came
		 * directly from the heap. */
		if(!tag->cache) {
			heap_free(tag, ROUND_UP(tag->size + sizeof(alloc_tag_t), PAGE_SIZE));
			return;
		}

		/* Free to the cache it came from. */
		slab_cache_free(tag->cache, tag);
	}
}

/** Initialize the allocator caches. */
__init_text void malloc_init(void) {
	char name[SLAB_NAME_MAX];
	size_t i, size;

	for(i = 0; i < ARRAY_SIZE(kmalloc_caches); i++) {
		size = (1 << (i + KMALLOC_CACHE_MIN));
		snprintf(name, SLAB_NAME_MAX, "kmalloc_%zu", size);
		name[SLAB_NAME_MAX - 1] = 0;

		kmalloc_caches[i] = slab_cache_create(name, size, 0, NULL, NULL,
		                                      NULL, 0, MM_FATAL);
	}
}
