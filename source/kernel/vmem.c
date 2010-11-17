/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Vmem resource allocator.
 *
 * Reference:
 * - Magazines and Vmem: Extending the Slab Allocator to Many CPUs and
 *   Arbitrary Resources.
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.6.8388
 *
 * Quick note about the boundary tag list: it is not sorted in span order
 * because doing so would mean that vmem_add_real() would be O(n), where n is
 * the number of tags in the list. Without keeping spans sorted, it is O(1),
 * just requiring the span to be placed on the end of the list. Segments under
 * a span, however, are sorted.
 */

#include <arch/memmap.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/kheap.h>
#include <mm/slab.h>

#include <proc/thread.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <console.h>
#include <dpc.h>
#include <fatal.h>
#include <kdbg.h>
#include <time.h>
#include <vmem.h>

#if CONFIG_VMEM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Limitations/settings. */
#define VMEM_REFILL_THRESHOLD	16		/**< Minimum number of boundary tags before refilling. */
#define VMEM_BOOT_TAG_COUNT	64		/**< Number of boundary tags to statically allocate. */
#define VMEM_RETRY_INTERVAL	SECS2USECS(1)	/**< Interval between retries when sleeping for space (in µs). */
#define VMEM_RETRY_MAX		30		/**< Maximum number of VMEM_RETRY_INTERVAL-long iterations. */
#define VMEM_REHASH_THRESHOLD	32		/**< Depth of a hash chain at which a rehash will be triggered. */
#define VMEM_HASH_MAX		8192		/**< Maximum size of the allocation hash table. */
#define VMEM_PERIODIC_INTERVAL	SECS2USECS(10)	/**< Interval for periodic maintenance. */

/** Vmem boundary tag structure. */
typedef struct vmem_btag {
	list_t header;				/**< Link to boundary tag list. */
	list_t s_link;				/**< Link to allocated/free list. */

	vmem_resource_t base;			/**< Start of the range the tag covers. */
	vmem_resource_t size;			/**< Size of the range. */
	struct vmem_btag *span;			/**< Parent span (for segments). */

	/** Type of the tag. */
	enum {
		VMEM_BTAG_SPAN,			/**< Span. */
		VMEM_BTAG_IMPORTED,		/**< Imported span. */
		VMEM_BTAG_FREE,			/**< Free segment. */
		VMEM_BTAG_ALLOC,		/**< Allocated segment. */
	} type;
} vmem_btag_t;

/** List of all arenas. */
static LIST_DECLARE(vmem_arenas);

/** Boundary tag allocation information. */
static LIST_DECLARE(vmem_btags);
static size_t vmem_btag_count;
static vmem_t vmem_btag_arena;

/** Lock to protect global vmem information. */
static MUTEX_DECLARE(vmem_lock, 0);

/** Statically allocated boundary tags to use during boot. */
static vmem_btag_t vmem_boot_tags[VMEM_BOOT_TAG_COUNT];

/** Periodiic maintenance timer. */
static timer_t vmem_maintenance_timer;

/** Allocate a new boundary tag structure.
 * @note		It is possible for this function to change the arena
 *			layout!
 * @param vmem		Arena that wants to allocate a tag.
 * @param vmflag	Allocation flags.
 * @return		Pointer to tag structure or NULL if cannot allocate. */
static vmem_btag_t *vmem_btag_alloc(vmem_t *vmem, int vmflag) {
	vmem_resource_t addr;
	vmem_btag_t *tag;
	size_t i;

	while(true) {
		mutex_lock(&vmem_lock);

		/* If there are more tags than the refill threshold or we are
		 * refilling the tag list at the moment then take a tag from
		 * the list. */
		if(vmem_btag_count) {
			if(vmflag & VM_REFILLING || vmem_btag_count > VMEM_REFILL_THRESHOLD) {
				assert(!list_empty(&vmem_btags));

				tag = list_entry(vmem_btags.next, vmem_btag_t, header);
				list_remove(&tag->header);
				vmem_btag_count--;

				mutex_unlock(&vmem_lock);
				return tag;
			}
		} else if(vmflag & VM_REFILLING) {
			fatal("Exhausted free boundary tags while refilling");
		}

		mutex_unlock(&vmem_lock);
		mutex_unlock(&vmem->lock);

		/* We need to allocate new boundary tags. Allocate a page from
		 * the tag arena and split it up into tags. */
		addr = vmem_alloc(&vmem_btag_arena, PAGE_SIZE, vmflag | VM_REFILLING);
		if(addr == 0) {
			mutex_lock(&vmem->lock);
			return NULL;
		}

		mutex_lock(&vmem_lock);

		tag = (vmem_btag_t *)((ptr_t)addr);
		for(i = 0; i < (PAGE_SIZE / sizeof(vmem_btag_t)); i++) {
			list_init(&tag[i].header);
			list_init(&tag[i].s_link);
			list_append(&vmem_btags, &tag[i].header);
			vmem_btag_count++;
		}

		mutex_unlock(&vmem_lock);
		mutex_lock(&vmem->lock);
	}
}

/** Free a boundary tag structure.
 * @param tag		Tag structure to free. */
static void vmem_btag_free(vmem_btag_t *tag) {
	assert(list_empty(&tag->s_link));

	mutex_lock(&vmem_lock);
	list_prepend(&vmem_btags, &tag->header);
	mutex_unlock(&vmem_lock);
}

/** Rehash a vmem arena.
 * @param _vmem		Pointer to arena to rehash. */
static void vmem_rehash(void *_vmem) {
	size_t new_size, prev_size, i;
	vmem_t *vmem = _vmem;
	list_t *table, *prev;
	vmem_btag_t *seg;
	uint32_t hash;

	/* Work out the new size of the hash: the next highest power of 2 from
	 * the current number of used segments. */
	new_size = MIN(vmem->used_segs, VMEM_HASH_MAX);
	new_size = MAX(new_size, VMEM_HASH_INITIAL);
	if(new_size & (new_size - 1)) {
		new_size = (size_t)1 << highbit(new_size);
	}

	if(new_size == vmem->alloc_hash_size) {
		return;
	}

	dprintf("vmem: rehashing arena %p(%s), new table size is %llu\n", vmem, vmem->name, new_size);

	/* Allocate and initialise the new table. */
	table = kmalloc(sizeof(list_t) * new_size, 0);
	if(!table) {
		vmem->rehash_requested = false;
		return;
	}

	for(i = 0; i < new_size; i++) {
		list_init(&table[i]);
	}

	mutex_lock(&vmem->lock);

	prev = vmem->alloc_hash;
	prev_size = vmem->alloc_hash_size;
	vmem->alloc_hash = table;
	vmem->alloc_hash_size = new_size;

	/* Add the entries from the old table to the new one. */
	for(i = 0; i < prev_size; i++) {
		LIST_FOREACH_SAFE(&prev[i], iter) {
			seg = list_entry(iter, vmem_btag_t, s_link);

			hash = fnv_hash_integer(seg->base) % new_size;
			list_append(&table[hash], &seg->s_link);
		}
	}

	vmem->rehash_requested = false;
	mutex_unlock(&vmem->lock);

	if(prev != vmem->initial_hash) {
		kfree(prev);
	}
}

/** Perform periodic maintenance on all arenas.
 * @param data		Unused. */
static bool vmem_maintenance(void *data) {
	vmem_t *vmem;

	LIST_FOREACH(&vmem_arenas, iter) {
		vmem = list_entry(iter, vmem_t, header);

		if(!vmem->rehash_requested) {
			vmem_rehash(vmem);
		}
	}

	return false;
}

/** Check if a freelist is empty.
 * @param vmem		Arena to check in.
 * @param list		List number.
 * @return		True if empty, false if not. */
static bool vmem_freelist_empty(vmem_t *vmem, int list) {
	if(!(vmem->free_map & ((vmem_resource_t)1 << list))) {
		return true;
	}

	assert(!list_empty(&vmem->free[list]));
	return false;
}

/** Add a segment to an arena's freelist.
 * @param vmem		Arena to modify.
 * @param tag		Segment to add. */
static void vmem_freelist_insert(vmem_t *vmem, vmem_btag_t *tag) {
	int list = highbit(tag->size) - 1;

	list_append(&vmem->free[list], &tag->s_link);
	vmem->free_map |= ((vmem_resource_t)1 << list);
}

/** Remove a segment from its freelist.
 * @param vmem		Arena to modify.
 * @param tag		Segment to remove. */
static void vmem_freelist_remove(vmem_t *vmem, vmem_btag_t *tag) {
	int list = highbit(tag->size) - 1;

	list_remove(&tag->s_link);
	if(list_empty(&vmem->free[list])) {
		vmem->free_map &= ~((vmem_resource_t)1 << list);
	}
}

/** Check if a span overlaps an existing span.
 * @param vmem		Arena to check in.
 * @param base		Start of span to check.
 * @param size		End of the span.
 * @return		True if span overlaps, false otherwise. */
static inline bool vmem_span_overlaps(vmem_t *vmem, vmem_resource_t base, vmem_resource_t end) {
	vmem_resource_t btend;
	vmem_btag_t *btag;

	LIST_FOREACH(&vmem->btags, iter) {
		btag = list_entry(iter, vmem_btag_t, header);

		btend = btag->base + btag->size;

		if(btag->type != VMEM_BTAG_SPAN && btag->type != VMEM_BTAG_IMPORTED) {
			continue;
		} else if(base >= btag->base && base < btend) {
			return true;
		} else if(end > btag->base && end <= btend) {
			return true;
		}
	}

	return false;
}

/** Real add span operation. Does not add a segment after the span.
 * @param vmem		Arena to add to.
 * @param base		Base of new span.
 * @param size		Size of the new span.
 * @param imported	Whether the span is imported.
 * @param vmflag	Allocation flags.
 * @return		Pointer to boundary tag on success, NULL on failure. */
static vmem_btag_t *vmem_add_real(vmem_t *vmem, vmem_resource_t base, vmem_resource_t size,
                                  bool imported, int vmflag) {
	vmem_btag_t *span;

	assert(!(base % vmem->quantum));
	assert(!(size % vmem->quantum));

	span = vmem_btag_alloc(vmem, vmflag);
	if(span == NULL) {
		return NULL;
	}

	span->base = base;
	span->size = size;
	span->span = NULL;
	span->type = (imported) ? VMEM_BTAG_IMPORTED : VMEM_BTAG_SPAN;

	vmem->total_size += size;

	/* Insert the span into the tag list. */
	list_append(&vmem->btags, &span->header);
	return span;
}

/** Find a free segment using best-fit.
 * @param vmem		Arena to search in.
 * @param size		Size of segment required.
 * @param minaddr	Minimum address of the allocation.
 * @param maxaddr	Maximum address of the end of the allocation.
 * @param list		Number of the lowest list that can contain a segment
 *			of the requested size.
 * @return		Pointer to segment tag if found, NULL if not. */
static vmem_btag_t *vmem_find_bestfit(vmem_t *vmem, vmem_resource_t size,
                                      vmem_resource_t minaddr, vmem_resource_t maxaddr,
                                      int list) {
	vmem_resource_t start, end;
	vmem_btag_t *seg;
	int i;

	/* Search through all the freelists large enough. */
	for(i = list; i < VMEM_FREELISTS; i++) {
		if(vmem_freelist_empty(vmem, i)) {
			continue;
		}

		/* Take the next tag off the list. */
		LIST_FOREACH(&vmem->free[i], iter) {
			seg = list_entry(iter, vmem_btag_t, s_link);
			end = seg->base + seg->size;

			/* Ensure that the segment satisfies the allocation
			 * constraints. */
			if(seg->size < size) {
				continue;
			} else if((end - 1) < minaddr) {
				continue;
			} else if(seg->base > (maxaddr - 1)) {
				continue;
			}

			/* Make sure we can actually fit. */
			start = MAX(seg->base, minaddr);
			end = MIN(end - 1, maxaddr - 1) + 1;
			if(size > (end - start)) {
				continue;
			}

			return seg;
		}
	}

	return NULL;
}

/** Find a free segment using instant-fit.
 * @param vmem		Arena to search in.
 * @param size		Size of segment required.
 * @param list		Number of the lowest list that can contain a segment
 *			of the requested size.
 * @return		Pointer to segment tag if found, NULL if not. */
static vmem_btag_t *vmem_find_instantfit(vmem_t *vmem, vmem_resource_t size, int list) {
	/* If the size is exactly a power of 2, then segments on freelist[n]
	 * are guaranteed to be big enough. Otherwise, use freelist[n + 1] so
	 * that we ensure that all segments we find are large enough. The free
	 * bitmap check will ensure that list does not go higher than the
	 * number of freelists. */
	if((size & (size - 1)) != 0 && vmem->free_map >> (list + 1)) {
		list++;
	}

	/* The rest is the same as best-fit. */
	return vmem_find_bestfit(vmem, size, 0, 0, list);
}

/** Find a free segment large enough for the given allocation.
 * @param vmem		Arena to search in.
 * @param size		Size of segment required.
 * @param minaddr	Minimum address of the allocation.
 * @param maxaddr	Maximum address of the end of the allocation.
 * @param vmflag	Allocation flags.
 * @return		Pointer to segment tag if found, NULL if not. */
static vmem_btag_t *vmem_find_segment(vmem_t *vmem, vmem_resource_t size,
                                      vmem_resource_t minaddr, vmem_resource_t maxaddr,
                                      int vmflag) {
	vmem_btag_t *seg, *split1 = NULL, *split2 = NULL;
	int list = highbit(size) - 1;

	assert(size);

	/* Don't perform an instant fit allocation if we have specific
	 * constraints. */
	if(minaddr || maxaddr) {
		vmflag |= VM_BESTFIT;
	}

	while(true) {
		/* Attempt to find a segment. */
		if(vmflag & VM_BESTFIT) {
			seg = vmem_find_bestfit(vmem, size, minaddr, maxaddr, list);
		} else {
			seg = vmem_find_instantfit(vmem, size, list);
		}

		if(!seg) {
			return NULL;
		}

		/* If splitting is necessary, then get hold of tags for us
		 * to use. Refilling the tag list can cause the arena layout
		 * to change, so we have to reattempt the allocation after
		 * this. */
		if(seg->base < minaddr && split1 == NULL) {
			split1 = vmem_btag_alloc(vmem, vmflag);
			if(split1 == NULL) {
				if(split2 != NULL) {
					vmem_btag_free(split2);
				}
				return NULL;
			}
			continue;
		}
		if(seg->size > size && split2 == NULL) {
			split2 = vmem_btag_alloc(vmem, vmflag);
			if(split2 == NULL) {
				if(split1 != NULL) {
					vmem_btag_free(split1);
				}
				return NULL;
			}
			continue;
		}

		/* Take the tag off the freelist before any splitting to ensure
		 * we do not cause any inconsistencies. */
		vmem_freelist_remove(vmem, seg);

		/* We have all the tags required, perform any splits needed. */
		if(seg->base < minaddr) {
			assert(split1);
			split1->base = seg->base;
			split1->size = minaddr - seg->base;
			split1->span = seg->span;
			split1->type = VMEM_BTAG_FREE;

			seg->base = minaddr;
			seg->size -= split1->size;
			list_add_before(&seg->header, &split1->header);
			vmem_freelist_insert(vmem, split1);
			split1 = NULL;
		}
		if(seg->size > size) {
			assert(split2);
			split2->base = seg->base + size;
			split2->size = seg->size - size;
			split2->span = seg->span;
			split2->type = VMEM_BTAG_FREE;

			seg->size = size;
			list_add_after(&seg->header, &split2->header);
			vmem_freelist_insert(vmem, split2);
			split2 = NULL;
		}

		/* Free tags that may no longer be needed - we could have
		 * allocated too many if a tag refill caused a layout change
		 * and made splitting no longer necessary. */
		if(split1 != NULL) {
			vmem_btag_free(split1);
		}
		if(split2 != NULL) {
			vmem_btag_free(split2);
		}
		seg->type = VMEM_BTAG_ALLOC;
		return seg;
	}
}

/** Attempt to import a span from the source arena.
 * @param vmem		Arena to import to.
 * @param size		Size of the span to import.
 * @param vmflag	Allocation flags.
 * @return		Segment for the imported span, or NULL on failure. */
static vmem_btag_t *vmem_import(vmem_t *vmem, vmem_resource_t size, int vmflag) {
	vmem_btag_t *span, *seg;
	vmem_resource_t ret;

	/* Unlock while we call afunc, so that we don't hold up any other
	 * calls that may take place on this arena if using MM_SLEEP. */
	mutex_unlock(&vmem->lock);
	ret = vmem->afunc(vmem->source, size, vmflag);
	mutex_lock(&vmem->lock);

	if(ret == 0) {
		return NULL;
	}

	/* Add the span and an allocated segment covering it. */
	span = vmem_add_real(vmem, ret, size, true, vmflag);
	if(span == NULL) {
		return NULL;
	}

	seg = vmem_btag_alloc(vmem, vmflag);
	if(seg == NULL) {
		vmem->total_size -= size;
		vmem_btag_free(span);
		mutex_unlock(&vmem->lock);
		vmem->ffunc(vmem->source, ret, size);
		mutex_lock(&vmem->lock);
		return NULL;
	}

	vmem->imported_size += size;

	seg->base = ret;
	seg->size = size;
	seg->span = span;
	seg->type = VMEM_BTAG_ALLOC;

	/* Insert the segment after the span. */
	list_add_after(&span->header, &seg->header);

	dprintf("vmem: imported span [0x%" PRIx64 ", 0x%" PRIx64 ") (vmem: %s, source: %s)\n",
		ret, ret + size, vmem->name, vmem->source->name);
	return seg;
}

/** Unimport a span if it is no longer required.
 * @param vmem		Arena to unimport from.
 * @param span		Span to unimport. */
static void vmem_unimport(vmem_t *vmem, vmem_btag_t *span) {
	vmem_resource_t base, size;
	vmem_btag_t *seg;

	assert(span);
	assert(span->type == VMEM_BTAG_IMPORTED);

	/* Check whether the span still has allocated segments. If we're
	 * followed by a free segment covering the entire span we're OK to
	 * unimport. */
	seg = list_entry(span->header.next, vmem_btag_t, header);
	if(seg->type != VMEM_BTAG_FREE || (seg->base != span->base && seg->size != span->size)) {
		return;
	}

	vmem->total_size -= span->size;
	vmem->imported_size -= span->size;

	/* Record what we're freeing as something may take the tag after we've
	 * freed it. */
	base = span->base;
	size = span->size;

	vmem_freelist_remove(vmem, seg);
	vmem_btag_free(seg);
	vmem_btag_free(span);

	mutex_unlock(&vmem->lock);
	vmem->ffunc(vmem->source, base, size);
	mutex_lock(&vmem->lock);

	dprintf("vmem: unimported span [0x%" PRIx64 ", 0x%" PRIx64 ") (vmem: %s, source: %s)\n",
		base, base + size, vmem->name, vmem->source->name);
}

/** Allocate a segment from a vmem arena.
 *
 * Allocates a segment from a vmem arena, importing a new span from the
 * source if necessary. The allocation behaviour can be modified by specifying
 * certain behaviour flags. The allocation is made to satisfy the specified
 * constraints. Because of this, it cannot use the quantum caches for the
 * arena, so they are bypassed. For this reason, allocations made with this
 * function MUST be freed using vmem_xfree(), which also bypasses the quantum
 * caches. If you do not have any special allocation constraints, you should
 * use vmem_alloc() to ensure that quantum caches will be used where
 * necessary.
 *
 * @todo		Implement the align, phase and nocross constraints.
 *
 * @note		One thing I'm not entirely sure on in this function
 *			is how minaddr/maxaddr are handled when it is necessary
 *			to import from the source arena. I have currently
 *			implemented it how Solaris' implementation does it -
 *			if a minaddr/maxaddr are specified, do not import from
 *			the source at all. However, this seems just a little
 *			bit odd to me. Actually attempting to handle it would
 *			be difficult, too...
 *
 * @param vmem		Arena to allocate from.
 * @param size		Size of the segment to allocate.
 * @param align		Alignment of allocation.
 * @param phase		Offset from alignment boundary.
 * @param nocross	Alignment boundary the allocation should not go across.
 * @param minaddr	Minimum start address of the allocation.
 * @param maxaddr	Highest end address of the allocation.
 * @param vmflag	Allocation flags.
 *
 * @return		Address of the allocation, or NULL on failure.
 */
vmem_resource_t vmem_xalloc(vmem_t *vmem, vmem_resource_t size,
                            vmem_resource_t align, vmem_resource_t phase,
                            vmem_resource_t nocross, vmem_resource_t minaddr,
                            vmem_resource_t maxaddr, int vmflag) {
	vmem_resource_t curr_size, hash, ret = 0;
	vmem_btag_t *seg;
	size_t count = 0;

	assert(vmem);
	assert(size > 0);
	assert(!(size % vmem->quantum));
	assert(!(minaddr % vmem->quantum));
	assert(!(maxaddr % vmem->quantum));

	mutex_lock(&vmem->lock);

	if(align != 0 || phase != 0 || nocross != 0) {
		fatal("Laziness has prevented implementation of these constraints. Sorry!");
	}

	/* Continuously loop until we can make the allocation. If MM_SLEEP is
	 * not set, this will break out once reclaiming cannot free any space
	 * in the arena. */
	while(true) {
		/* First try to find a free segment in the arena. */
		seg = vmem_find_segment(vmem, size, minaddr, maxaddr, vmflag);
		if(seg) {
			break;
		}

		/* If there is a source arena and the allocation does not have
		 * address constraints, try importing from it. Don't need to
		 * bother sleeping if we cannot import from the source - the
		 * allocation flags get passed down so waiting should take
		 * place at the arena at the end of the chain. */
		if(vmem->source && minaddr == 0 && maxaddr == 0) {
			seg = vmem_import(vmem, size, vmflag);
			break;
		}

		/* If the resource type is not 0, attempt to reclaim space. */
		if(vmem->type) {
			curr_size = vmem->used_size;
			mutex_unlock(&vmem->lock);

			lrm_reclaim(vmem->type);

			mutex_lock(&vmem->lock);
			if(vmem->used_size < curr_size) {
				continue;
			}
		}

		/* Could not reclaim any space. Break out if not sleeping. */
		if(!(vmflag & MM_SLEEP)) {
			break;
		}

		/* Give up if we've waited for too long. */
		if(count++ == VMEM_RETRY_MAX) {
			fatal("Exhausted available space in %p(%s)", vmem, vmem->name);
		}

		/* Wait for at most the configured interval and try again. */
		kprintf(LOG_DEBUG, "vmem: waiting for space in %p(%s)...\n", vmem, vmem->name);
		condvar_wait_etc(&vmem->space_cvar, &vmem->lock, NULL, VMEM_RETRY_INTERVAL, 0);
	}

	if(seg) {
		/* Add to allocation hash table. */
		hash = fnv_hash_integer(seg->base) % vmem->alloc_hash_size;
		list_append(&vmem->alloc_hash[hash], &seg->s_link);

		vmem->used_size += size;
		vmem->used_segs++;
		vmem->alloc_count++;
		ret = seg->base;
	} else if(vmflag & MM_FATAL) {
		fatal("Could not perform mandatory allocation on arena %p(%s)",
		      vmem, vmem->name);
	}
	mutex_unlock(&vmem->lock);
	return ret;
}

/** Free a segment to a vmem arena.
 *
 * Frees a previously allocated segment in a vmem arena, bypassing the
 * quantum caches. If the allocation was originally made using vmem_alloc(),
 * use vmem_free() instead.
 *
 * @param vmem		Arena to allocate from.
 * @param addr		Address of the allocation to free.
 * @param size		Size of the segment to allocate.
 */
void vmem_xfree(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size) {
	vmem_btag_t *tag, *exist;
	uint32_t hash;
	size_t i = 0;

	assert(vmem);
	assert((size % vmem->quantum) == 0);

	mutex_lock(&vmem->lock);

	/* Look for the allocation on the allocation hash table. */
	hash = fnv_hash_integer(addr) % vmem->alloc_hash_size;
	LIST_FOREACH(&vmem->alloc_hash[hash], iter) {
		tag = list_entry(iter, vmem_btag_t, s_link);

		assert(tag->type == VMEM_BTAG_ALLOC);
		assert(tag->span);

		if(tag->base != addr) {
			i++;
			continue;
		} else if(tag->size != size) {
			fatal("Bad vmem_xfree(%s): size: %" PRIu64 ", segment: %" PRIu64,
			      vmem->name, size, tag->size);
		}

		/* Although we periodically rehash all arenas, if we've exceeded
		 * a certain chain depth in the search for the segment, trigger
		 * a rehash. This is because under heavy load, we don't want to
		 * have to wait for the periodic rehash. Don't make a request if
		 * we have already made one that has not been completed yet, to
		 * prevent flooding the DPC manager with requests. */
		if(i >= VMEM_REHASH_THRESHOLD && !vmem->rehash_requested && dpc_inited()) {
			dprintf("vmem: saw %zu segments in search on chain %u on %p(%s), triggering rehash\n",
			        i, hash, vmem, vmem->name);
			vmem->rehash_requested = true;
			dpc_request(vmem_rehash, vmem);
		}

		tag->type = VMEM_BTAG_FREE;

		vmem->used_size -= tag->size;
		vmem->used_segs--;

		/* Coalesce adjacent free segments. */
		if(tag->header.next != &vmem->btags) {
			exist = list_entry(tag->header.next, vmem_btag_t, header);
			if(exist->type == VMEM_BTAG_FREE) {
				assert((tag->base + tag->size) == exist->base);
				tag->size += exist->size;
				vmem_freelist_remove(vmem, exist);
				vmem_btag_free(exist);
			}
		}

		/* Can't be the list header because there should be a span before. */
		assert(tag->header.prev != &vmem->btags);

		exist = list_entry(tag->header.prev, vmem_btag_t, header);
		if(exist->type == VMEM_BTAG_FREE) {
			assert((exist->base + exist->size) == tag->base);
			tag->base = exist->base;
			tag->size += exist->size;
			vmem_freelist_remove(vmem, exist);
			vmem_btag_free(exist);
		}

		vmem_freelist_insert(vmem, tag);

		/* Check if the span can be unimported. */
		if(vmem->source && tag->span->type == VMEM_BTAG_IMPORTED) {
			vmem_unimport(vmem, tag->span);
		} else {
			condvar_broadcast(&vmem->space_cvar);
		}

		mutex_unlock(&vmem->lock);
		return;
	}

	fatal("Bad vmem_free(%s): cannot find segment 0x%" PRIx64, vmem->name, addr);
}

/** Allocate a segment from a vmem arena.
 *
 * Allocates a segment from a vmem arena, importing a new span from the
 * source if necessary. The allocation behaviour can be modified by specifying
 * certain behaviour flags.
 *
 * @param vmem		Arena to allocate from.
 * @param size		Size of the segment to allocate.
 * @param vmflag	Allocation flags.
 *
 * @return		Address of the allocation, or NULL on failure.
 */
vmem_resource_t vmem_alloc(vmem_t *vmem, vmem_resource_t size, int vmflag) {
	assert(vmem);
	assert(size > 0);
	assert(!(size % vmem->quantum));

	/* Use the quantum caches if possible. */
	if(size <= vmem->qcache_max) {
		return (vmem_resource_t)((ptr_t)slab_cache_alloc(vmem->qcache[(size - 1) >> vmem->qshift],
		                                                 vmflag & MM_FLAG_MASK));
	}

	return vmem_xalloc(vmem, size, 0, 0, 0, 0, 0, vmflag);
}

/** Free a segment to a vmem arena.
 *
 * Frees a previously allocated segment in a vmem arena. If the allocation was
 * originally made using vmem_xalloc(), use vmem_xfree() instead.
 *
 * @param vmem		Arena to allocate from.
 * @param addr		Address of the allocation to free.
 * @param size		Size of the segment to allocate.
 */
void vmem_free(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size) {
	assert(vmem);
	assert((size % vmem->quantum) == 0);

	if(size <= vmem->qcache_max) {
		slab_cache_free(vmem->qcache[(size - 1) >> vmem->qshift], (void *)((ptr_t)addr));
		return;
	}

	vmem_xfree(vmem, addr, size);
}

/** Add a new span to an arena.
 * @param vmem		Arena to add to.
 * @param base		Base of the new span.
 * @param size		Size of the new span.
 * @param vmflag	Allocation flags.
 * @return		Whether the span was added. Failure can only occur if
 *			MM_SLEEP/MM_FATAL are not specified. */
bool vmem_add(vmem_t *vmem, vmem_resource_t base, vmem_resource_t size, int vmflag) {
	vmem_btag_t *span, *seg;

	mutex_lock(&vmem->lock);

	/* The new span should not overlap an existing span. */
	if(vmem_span_overlaps(vmem, base, base + size)) {
		fatal("Tried to add overlapping span [0x%" PRIx64 ", 0x%" PRIx64 ") to %p",
			base, base + size, vmem);
	}

	/* Create the span itself. */
	span = vmem_add_real(vmem, base, size, false, vmflag);
	if(span == NULL) {
		mutex_unlock(&vmem->lock);
		return false;
	}

	/* Create a free segment. */
	seg = vmem_btag_alloc(vmem, vmflag);
	if(seg == NULL) {
		vmem_btag_free(span);
		mutex_unlock(&vmem->lock);
		return false;
	}

	seg->base = base;
	seg->size = size;
	seg->span = span;
	seg->type = VMEM_BTAG_FREE;

	/* Place the segment after the span and add it to the freelists. */
	list_add_after(&span->header, &seg->header);
	vmem_freelist_insert(vmem, seg);

	dprintf("vmem: added span [0x%" PRIx64 ", 0x%" PRIx64 ") to %p(%s)\n",
	        base, base + size, vmem, vmem->name);
	mutex_unlock(&vmem->lock);
	return true;
}

/** Initialise a vmem arena.
 *
 * Initialises a vmem arena and creates an initial span/free segment if the
 * given size is non-zero.
 *
 * @param vmem		Arena to initialise.
 * @param name		Name of the arena for debugging purposes.
 * @param base		Start of the initial span.
 * @param size		Size of the initial span.
 * @param quantum	Allocation granularity.
 * @param afunc		Function to call to import from the source.
 * @param ffunc		Function to call to free to the source.
 * @param source	Arena backing this arena.
 * @param qcache_max	Maximum size to cache.
 * @param flags		Behaviour flags for the arena.
 * @param type		Type of the resource the arena is allocating, or 0.
 * @param vmflag	Allocation flags.
 *
 * @return		Whether the arena was created successfully.
 */
bool vmem_early_create(vmem_t *vmem, const char *name, vmem_resource_t base, vmem_resource_t size,
                       size_t quantum, vmem_afunc_t afunc, vmem_ffunc_t ffunc, vmem_t *source,
                       size_t qcache_max, int flags, uint32_t type, int vmflag) {
	char qcname[SLAB_NAME_MAX];
	size_t i;

	assert(vmem);
	assert(quantum);
	assert(!(base % quantum));
	assert(!(size % quantum));
	assert(!(qcache_max % quantum));
	assert(source != vmem);
	assert(!source || (afunc && ffunc));

	/* Impose a limit on the number of quantum caches. */
	if(qcache_max > (quantum * VMEM_QCACHE_MAX)) {
		qcache_max = quantum * VMEM_QCACHE_MAX;
	}

	list_init(&vmem->btags);
	list_init(&vmem->header);
	list_init(&vmem->children);
	list_init(&vmem->parent_link);
	mutex_init(&vmem->lock, "vmem_arena_lock", 0);
	condvar_init(&vmem->space_cvar, "vmem_space_cvar");

	/* Initialise freelists and the initial allocation hash table. */
	for(i = 0; i < VMEM_FREELISTS; i++) {
		list_init(&vmem->free[i]);
	}
	for(i = 0; i < VMEM_HASH_INITIAL; i++) {
		list_init(&vmem->initial_hash[i]);
	}

	vmem->quantum = quantum;
	vmem->qcache_max = qcache_max;
	vmem->qshift = highbit(quantum) - 1;
	vmem->type = type;
	vmem->free_map = 0;
	vmem->alloc_hash = vmem->initial_hash;
	vmem->alloc_hash_size = VMEM_HASH_INITIAL;
	vmem->rehash_requested = false;
	vmem->afunc = afunc;
	vmem->ffunc = ffunc;
	vmem->source = source;
	vmem->total_size = 0;
	vmem->used_size = 0;
	vmem->imported_size = 0;
	vmem->used_segs = 0;
	vmem->alloc_count = 0;
	vmem->flags = flags;
	strncpy(vmem->name, name, VMEM_NAME_MAX);
	vmem->name[VMEM_NAME_MAX - 1] = 0;

	/* Create the quantum caches. */
	if(vmem->qcache_max) {
		memset(vmem->qcache, 0, sizeof(vmem->qcache));
		for(i = 0; i < (vmem->qcache_max / vmem->quantum); i++) {
			snprintf(qcname, SLAB_NAME_MAX, "%s_%zu", vmem->name, (i + 1) * vmem->quantum);
			qcname[SLAB_NAME_MAX - 1] = 0;

			vmem->qcache[i] = slab_cache_create(qcname, (i + 1) * vmem->quantum,
			                                    vmem->quantum, NULL, NULL, NULL,
			                                    vmem, SLAB_CACHE_QCACHE, 0);
			if(vmem->qcache[i] == NULL) {
				goto fail;
			}
		}
	}

	/* Add initial span, if any.*/
	if(size > 0) {
		if(!vmem_add(vmem, base, size, vmflag & ~MM_FATAL)) {
			goto fail;
		}
	}

	/* Add the arena to the source's child list (for the benefit of the
	 * KDBG command), and to the global arena list. */
	if(source) {
		mutex_lock(&source->lock);
		list_append(&source->children, &vmem->parent_link);
		mutex_unlock(&source->lock);
	}

	mutex_lock(&vmem_lock);
	list_append(&vmem_arenas, &vmem->header);
	mutex_unlock(&vmem_lock);

	dprintf("vmem: created arena %p(%s) (quantum: %zu, source: %p)\n",
		vmem, vmem->name, quantum, source);
	return true;
fail:
	if(vmflag & MM_FATAL) {
		fatal("Could not initialise required arena %s", vmem->name);
	}

	/* Destroy the quantum caches. */
	for(i = 0; i < (vmem->qcache_max / vmem->quantum); i++) {
		if(vmem->qcache[i]) {
			slab_cache_destroy(vmem->qcache[i]);
		}
	}
	return false;	
}

/** Allocate and initialise a vmem arena.
 *
 * Allocates a new vmem arena and creates an initial span/free segment if the
 * given size is non-zero.
 *
 * @param name		Name of the arena for debugging purposes.
 * @param base		Start of the initial span.
 * @param size		Size of the initial span.
 * @param quantum	Allocation granularity.
 * @param afunc		Function to call to import from the source.
 * @param ffunc		Function to call to free to the source.
 * @param source	Arena backing this arena.
 * @param qcache_max	Maximum size to cache.
 * @param flags		Behaviour flags for the arena.
 * @param type		Type of the resource the arena is allocating, or 0.
 * @param vmflag	Allocation flags.
 *
 * @return		Pointer to arena on success, NULL on failure.
 */
vmem_t *vmem_create(const char *name, vmem_resource_t base, vmem_resource_t size, size_t quantum,
                    vmem_afunc_t afunc, vmem_ffunc_t ffunc, vmem_t *source, size_t qcache_max,
                    int flags, uint32_t type, int vmflag) {
	vmem_t *vmem;

	vmem = kmalloc(sizeof(vmem_t), vmflag & MM_FLAG_MASK);
	if(vmem == NULL) {
		return NULL;
	}

	if(!vmem_early_create(vmem, name, base, size, quantum, afunc, ffunc, source,
	                      qcache_max, flags, type, vmflag)) {
		kfree(vmem);
		return NULL;
	}

	return vmem;
}

/** Find a vmem arena by name.
 * @param name		Name of arena to find.
 * @return		Pointer to arena found or NULL if not. */
static vmem_t *vmem_find_arena(const char *name) {
	vmem_t *vmem;

	LIST_FOREACH(&vmem_arenas, iter) {
		vmem = list_entry(iter, vmem_t, header);

		if(!(vmem->flags & VMEM_PRIVATE)) {
			if(strcmp(vmem->name, name) == 0) {
				return vmem;
			}
		}
	}

	return NULL;
}

/** Dump vmem arenas in a list.
 * @param header	List header to dump from.
 * @param indent	Number of spaces to indent the name. */
static void vmem_dump_list(list_t *header, int indent) {
	vmem_t *vmem;

	LIST_FOREACH(header, iter) {
		if(header == &vmem_arenas) {
			vmem = list_entry(iter, vmem_t, header);
			if(vmem->source) {
				continue;
			}
		} else {
			vmem = list_entry(iter, vmem_t, parent_link);
		}

		if(vmem->flags & VMEM_PRIVATE) {
			continue;
		}

		kprintf(LOG_NONE, "%*s%-*s %-4u %-16" PRIu64 " %-16" PRIu64 " %zu\n",
		        indent, "", VMEM_NAME_MAX - indent, vmem->name, vmem->type,
		        vmem->total_size, vmem->used_size, vmem->alloc_count);
		vmem_dump_list(&vmem->children, indent + 2);
	}
}

/** KDBG vmem information command.
 *
 * When supplied with no arguments, will give a list of all vmem arenas.
 * Otherwise, displays information about the specified arena.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_vmem(int argc, char **argv) {
	char *name = NULL;
	vmem_btag_t *btag;
	unative_t addr;
	vmem_t *vmem;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [arena]\n\n", argv[0]);

		kprintf(LOG_NONE, "When supplied with no arguments, prints a tree of all vmem arenas in the\n");
		kprintf(LOG_NONE, "system. Otherwise, prints information about and list of spans/segments in\n");
		kprintf(LOG_NONE, "the specified arena. The arena can be specified as an address expression\n");
		kprintf(LOG_NONE, "(e.g. %s &kheap_arena) or as an arena name (e.g. %s \"kheap\").\n", argv[0], argv[0]);

		return KDBG_OK;
	}

	/* If no arguments specified dump a tree of all arenas. */
	if(argc < 2) {
		kprintf(LOG_NONE, "Name                      Type Size             Used             Allocations\n");
		kprintf(LOG_NONE, "====                      ==== ====             ====             ===========\n");

		/* Print a list of arenas. */
		vmem_dump_list(&vmem_arenas, 0);
		return KDBG_OK;
	}

	if(kdbg_parse_expression(argv[1], &addr, &name) != KDBG_OK) {
		return KDBG_FAIL;
	}

	/* If a string was provided then do a lookup by name. */
	if(name != NULL) {
		if(strlen(name) >= VMEM_NAME_MAX || !(vmem = vmem_find_arena(name))) {
			kprintf(LOG_NONE, "Arena '%s' not found\n", name);
			return KDBG_FAIL;
		}
	} else {
		vmem = (vmem_t *)((ptr_t)addr);
	}

	/* Print out basic information. */
	kprintf(LOG_NONE, "Arena %p: %s\n", vmem, vmem->name);
	kprintf(LOG_NONE, "============================================================\n");
	kprintf(LOG_NONE, "Quantum: %zu  Size: %" PRIu64 "  Used: %" PRIu64 "  Allocations: %zu\n",
	        vmem->quantum, vmem->total_size, vmem->used_size, vmem->alloc_count);
	kprintf(LOG_NONE, "Hash: %p  Hash Size: %zu  Used Segments: %llu\n",
	        vmem->alloc_hash, vmem->alloc_hash_size, vmem->used_segs);
	kprintf(LOG_NONE, "Locked: %d (%" PRId32 ")\n", atomic_get(&vmem->lock.locked),
	        (vmem->lock.holder) ? vmem->lock.holder->id : -1);
	if(vmem->source) {
		kprintf(LOG_NONE, "Source: %p(%s)  Imported: %" PRIu64 "\n\n",
		        vmem->source, vmem->source->name, vmem->imported_size);
	} else {
		kprintf(LOG_NONE, "\n");
	}

	/* Print out a span/segment list. */
	kprintf(LOG_NONE, "Base                 End                  Type\n");
	kprintf(LOG_NONE, "====                 ===                  ====\n");
	LIST_FOREACH(&vmem->btags, iter) {
		btag = list_entry(iter, vmem_btag_t, header);

		if(btag->type == VMEM_BTAG_SPAN || btag->type == VMEM_BTAG_IMPORTED) {
			kprintf(LOG_NONE, "0x%016" PRIx64 "   0x%016" PRIx64 "   Span%s\n",
			            btag->base, btag->base + btag->size,
			            (btag->type == VMEM_BTAG_IMPORTED) ? " (Imported)" : "");
		} else {
			kprintf(LOG_NONE, "  0x%016" PRIx64 "   0x%016" PRIx64 " Segment %s\n",
			            btag->base, btag->base + btag->size,
			            (btag->type == VMEM_BTAG_FREE) ? "(Free)" : "(Allocated)");
		}
	}

	return KDBG_OK;
}

/** Add the initial tags to the boundary tag list. */
void __init_text vmem_early_init(void) {
	size_t i;

	for(i = 0; i < VMEM_BOOT_TAG_COUNT; i++) {
		list_init(&vmem_boot_tags[i].header);
		list_init(&vmem_boot_tags[i].s_link);
		list_append(&vmem_btags, &vmem_boot_tags[i].header);
		vmem_btag_count++;
	}
}

/** Create the boundary tag arena. */
void __init_text vmem_init(void) {
	/* Create the boundary tag arena. */
	vmem_early_create(&vmem_btag_arena, "vmem_btag_arena", 0, 0, PAGE_SIZE,
	                  kheap_anon_afunc, kheap_anon_ffunc, &kheap_raw_arena, 0,
	                  0, 0, MM_FATAL);
}

/** Start the periodic maintenance timer. */
void __init_text vmem_late_init(void) {
	timer_init(&vmem_maintenance_timer, vmem_maintenance, NULL, TIMER_THREAD);
	timer_start(&vmem_maintenance_timer, VMEM_PERIODIC_INTERVAL, TIMER_PERIODIC);
}
