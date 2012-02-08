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
 * Allocations are tracked using an alloc_btag_t structure, which is placed
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
typedef struct alloc_btag {
	unative_t size;			/**< Size of the allocation. */
	slab_cache_t *cache;		/**< Pointer to cache for allocation. */
} alloc_btag_t;

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
	size_t total = size + sizeof(alloc_btag_t), idx;
	alloc_btag_t *addr;

	/* Use the slab caches where possible. */
	if(total <= (1 << KMALLOC_CACHE_MAX)) {
		/* If exactly a power-of-two, then highbit(total) will work,
		 * else we want the next size up. Remember that the highbit
		 * function returns (log2(n) + 1). */
		idx = ((total & (total - 1)) == 0) ? highbit(total) - 1 : highbit(total);
		if(idx < KMALLOC_CACHE_MIN) {
			idx = KMALLOC_CACHE_MIN;
		}
		idx -= KMALLOC_CACHE_MIN;

		addr = slab_cache_alloc(kmalloc_caches[idx], kmflag);
		if(addr == NULL) {
			return NULL;
		}

		addr->size = size;
		addr->cache = kmalloc_caches[idx];
		return &addr[1];
	}

	/* Fall back on the kernel heap. */
	addr = heap_alloc(ROUND_UP(total, PAGE_SIZE), (kmflag & MM_FLAG_MASK) & ~MM_FATAL);
	if(addr == NULL) {
		if(kmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation (%zu bytes)", size);
		}
		return NULL;
	}

	addr->size = size;
	addr->cache = NULL;
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
	alloc_btag_t *btag = (alloc_btag_t *)(addr - sizeof(alloc_btag_t));
	void *ret;

	if(addr == NULL) {
		return kmalloc(size, kmflag);
	} else if(btag->size == size) {
		return addr;
	}

	/* Make a new allocation. */
	ret = kmalloc(size, kmflag);
	if(ret == NULL) {
		return ret;
	}

	/* Copy the block data using the smallest of the two sizes. */
	memcpy(ret, addr, MIN(btag->size, size));

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
	alloc_btag_t *btag;

	if(addr) {
		btag = (alloc_btag_t *)(addr - sizeof(alloc_btag_t));

		/* If the cache pointer is not set, assume the allocation came
		 * directly from the heap. */
		if(btag->cache == NULL) {
			heap_free(btag, ROUND_UP(btag->size + sizeof(alloc_btag_t), PAGE_SIZE));
			return;
		}

		/* Free to the cache it came from. */
		slab_cache_free(btag->cache, btag);
	}
}

/** Initialize the allocator caches. */
__init_text void malloc_init(void) {
	char name[SLAB_NAME_MAX];
	size_t i, size;

	/* Create all of the cache structures. */
	for(i = 0; i < ARRAY_SIZE(kmalloc_caches); i++) {
		size = (1 << (i + KMALLOC_CACHE_MIN));

		snprintf(name, SLAB_NAME_MAX, "kmalloc_%zu", size);
		name[SLAB_NAME_MAX - 1] = 0;

		kmalloc_caches[i] = slab_cache_create(name, size, 0, NULL, NULL,
		                                      NULL, 0, 0);
		if(kmalloc_caches[i] == NULL) {
			fatal("Could not create malloc cache: %s", name);
		}
	}
}
