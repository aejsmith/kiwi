/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Vmem resource allocator.
 *
 * Reference:
 * - Magazines and Vmem: Extending the Slab Allocator to Many CPUs and
 *   Arbitrary Resources.
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.6.8388
 *
 * Quick note about the boundary tag list: it is not sorted in span order
 * because doing so would mean that vmem_add_internal() would be O(n), where n
 * is the number of tags in the list. Without keeping spans sorted, it is O(1),
 * just requiring the span to be placed on the end of the list. Segments under
 * a span, however, are sorted.
 */

#include <arch/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/kheap.h>
#include <mm/page.h>
#include <mm/slab.h>

#include <proc/thread.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <console.h>
#include <dpc.h>
#include <kdbg.h>
#include <status.h>
#include <time.h>
#include <vmem.h>

#if CONFIG_VMEM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Limitations/settings. */
#define VMEM_REFILL_THRESHOLD	16		/**< Minimum number of boundary tags before refilling. */
#define VMEM_BOOT_TAG_COUNT	128		/**< Number of boundary tags to statically allocate. */
#define VMEM_RETRY_INTERVAL	SECS2USECS(1)	/**< Interval between retries when sleeping for space (in µs). */
#define VMEM_RETRY_MAX		30		/**< Maximum number of VMEM_RETRY_INTERVAL-long iterations. */
#define VMEM_REHASH_THRESHOLD	32		/**< Depth of a hash chain at which a rehash will be triggered. */
#define VMEM_HASH_MAX		8192		/**< Maximum size of the allocation hash table. */
#define VMEM_PERIODIC_INTERVAL	SECS2USECS(10)	/**< Interval for periodic maintenance. */

/** Vmem boundary tag structure. */
typedef struct vmem_btag {
	list_t tag_link;			/**< Link to boundary tag list. */
	list_t af_link;				/**< Link to allocated/free list. */

	vmem_resource_t base;			/**< Start of the range the tag covers. */
	vmem_resource_t size;			/**< Size of the range. */
	struct vmem_btag *span;			/**< Parent span (for segments). */
	unative_t flags;			/**< Flags for the tag. */
} vmem_btag_t;

/** Flags for boundary tags. */
#define VMEM_BTAG_SPAN		0x1		/**< Span. */
#define VMEM_BTAG_SEGMENT	0x2		/**< Segment. */
#define VMEM_BTAG_TYPE		0x3		/**< Type mask. */
#define VMEM_BTAG_ALLOC		0x4		/**< Segment is allocated. */
#define VMEM_BTAG_IMPORT	0x8		/**< Span was imported. */
#define VMEM_BTAG_XIMPORT	0x10		/**< Span was imported with xalloc(). */

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
 *			layout for VMEM_REFILL arenas.
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

				tag = list_entry(vmem_btags.next, vmem_btag_t, tag_link);
				list_remove(&tag->tag_link);
				vmem_btag_count--;

				mutex_unlock(&vmem_lock);
				return tag;
			}
		} else if(vmflag & VM_REFILLING) {
			fatal("Exhausted free boundary tags while refilling");
		}

		mutex_unlock(&vmem_lock);
		mutex_unlock(&vmem->lock);

		/* We want to protect against multiple threads trying to do a
		 * boundary tag allocation at the same time, as this could
		 * cause the free tag set we leave for use during the refill to
		 * be depleted. We cannot, however, have a different lock for
		 * this as this could cause deadlocks with the kernel page map
		 * lock. So, we use the kernel page map lock to achieve
		 * serialisation of refills. */
		page_map_lock(&kernel_page_map);

		addr = vmem_alloc(&vmem_btag_arena, PAGE_SIZE, vmflag | VM_REFILLING);
		if(addr == 0) {
			page_map_unlock(&kernel_page_map);
			mutex_lock(&vmem->lock);
			return NULL;
		}

		page_map_unlock(&kernel_page_map);
		mutex_lock(&vmem_lock);

		tag = (vmem_btag_t *)((ptr_t)addr);
		for(i = 0; i < (PAGE_SIZE / sizeof(vmem_btag_t)); i++) {
			list_init(&tag[i].tag_link);
			list_init(&tag[i].af_link);
			list_append(&vmem_btags, &tag[i].tag_link);
			vmem_btag_count++;
		}

		mutex_unlock(&vmem_lock);
		mutex_lock(&vmem->lock);
	}
}

/** Free a boundary tag structure.
 * @param tag		Tag structure to free. */
static void vmem_btag_free(vmem_btag_t *tag) {
	assert(list_empty(&tag->af_link));

	mutex_lock(&vmem_lock);
	list_prepend(&vmem_btags, &tag->tag_link);
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

	/* Don't do anything if we're low on boundary tags. */
	if(vmem_btag_count <= VMEM_REFILL_THRESHOLD) {
		return;
	}

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
			seg = list_entry(iter, vmem_btag_t, af_link);

			hash = fnv_hash_integer(seg->base) % new_size;
			list_append(&table[hash], &seg->af_link);
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

	list_append(&vmem->free[list], &tag->af_link);
	vmem->free_map |= ((vmem_resource_t)1 << list);
}

/** Remove a segment from its freelist.
 * @param vmem		Arena to modify.
 * @param tag		Segment to remove. */
static void vmem_freelist_remove(vmem_t *vmem, vmem_btag_t *tag) {
	int list = highbit(tag->size) - 1;

	list_remove(&tag->af_link);
	if(list_empty(&vmem->free[list])) {
		vmem->free_map &= ~((vmem_resource_t)1 << list);
	}
}

/** Internal add span operation, does not add a segment after the span.
 * @param vmem		Arena to add to.
 * @param base		Base of new span.
 * @param size		Size of the new span.
 * @param flags		Extra flags for the span.
 * @param vmflag	Allocation flags.
 * @return		Pointer to boundary tag on success, NULL on failure. */
static vmem_btag_t *vmem_add_internal(vmem_t *vmem, vmem_resource_t base, vmem_resource_t size,
                                      unative_t flags, int vmflag) {
	vmem_btag_t *span;

	assert(!(base % vmem->quantum));
	assert(!(size % vmem->quantum));

	span = vmem_btag_alloc(vmem, vmflag);
	if(unlikely(!span)) {
		return NULL;
	}

	span->base = base;
	span->size = size;
	span->span = NULL;
	span->flags = VMEM_BTAG_SPAN | flags;

	vmem->total_size += size;

	/* Insert the span into the tag list. */
	list_append(&vmem->btags, &span->tag_link);
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
			seg = list_entry(iter, vmem_btag_t, af_link);
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

	/* Don't perform an instant fit allocation if we have specific address
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
		 * to use. For arenas in the refill allocation path, refilling
		 * the tag list can cause the arena layout to change, so we
		 * have to reattempt the allocation after this. Note that
		 * rechecking for a free segment is cheaper than allocating a
		 * tag unnecessarily, so we leave tag allocation until here
		 * rather than blindly allocating 2 tags at the start of the
		 * function in case they're needed. */
		if(seg->base < minaddr && !split1) {
			split1 = vmem_btag_alloc(vmem, vmflag);
			if(unlikely(!split1)) {
				if(split2) {
					vmem_btag_free(split2);
				}
				return NULL;
			}
			if(vmem->flags & VMEM_REFILL) {
				continue;
			}
		}
		if(seg->size > size && !split2) {
			split2 = vmem_btag_alloc(vmem, vmflag);
			if(unlikely(!split2)) {
				if(split1) {
					vmem_btag_free(split1);
				}
				return NULL;
			}
			if(vmem->flags & VMEM_REFILL) {
				continue;
			}
		}

		vmem_freelist_remove(vmem, seg);

		/* Perform any splits needed. */
		if(seg->base < minaddr) {
			assert(split1);
			split1->base = seg->base;
			split1->size = minaddr - seg->base;
			split1->span = seg->span;
			split1->flags = VMEM_BTAG_SEGMENT;

			seg->base = minaddr;
			seg->size -= split1->size;
			list_add_before(&seg->tag_link, &split1->tag_link);
			vmem_freelist_insert(vmem, split1);
			split1 = NULL;
		}
		if(seg->size > size) {
			assert(split2);
			split2->base = seg->base + size;
			split2->size = seg->size - size;
			split2->span = seg->span;
			split2->flags = VMEM_BTAG_SEGMENT;

			seg->size = size;
			list_add_after(&seg->tag_link, &split2->tag_link);
			vmem_freelist_insert(vmem, split2);
			split2 = NULL;
		}

		seg->flags |= VMEM_BTAG_ALLOC;

		/* Free tags that weren't needed. */
		if(split1) {
			vmem_btag_free(split1);
		}
		if(split2) {
			vmem_btag_free(split2);
		}

		return seg;
	}
}

/** Attempt to import a span from the source arena.
 * @param vmem		Arena to import to.
 * @param size		Size of the span to import.
 * @param vmflag	Allocation flags.
 * @return		Segment for the imported span, or NULL on failure. */
static vmem_btag_t *vmem_import(vmem_t *vmem, vmem_resource_t size, vmem_resource_t align,
                                vmem_resource_t nocross, vmem_resource_t minaddr,
                                vmem_resource_t maxaddr, int vmflag) {
	vmem_btag_t *span, *seg;
	vmem_resource_t ret;
	unative_t flags;

	mutex_unlock(&vmem->lock);

	/* If we have any allocation constraints, pass them to the source. The
	 * tag is marked with the XIMPORT flag, to ensure that xfree() is used
	 * to unimport if required. */
	flags = VMEM_BTAG_IMPORT;
	if(align || nocross || minaddr || maxaddr) {
		flags |= VMEM_BTAG_XIMPORT;
		ret = vmem_xalloc(vmem->source, size, align, nocross, minaddr, maxaddr, vmflag);
	} else {
		ret = vmem_alloc(vmem->source, size, vmflag);
	}

	if(unlikely(!ret)) {
		mutex_lock(&vmem->lock);
		return NULL;
	}

	/* Call the import callback on the imported span. */
	if(vmem->import) {
		if(vmem->import(ret, size, vmflag) != STATUS_SUCCESS) {
			goto fail2;
		}
	}

	mutex_lock(&vmem->lock);

	/* Add the span and an allocated segment covering it. */
	span = vmem_add_internal(vmem, ret, size, flags, vmflag);
	if(unlikely(!span)) {
		goto fail1;
	}
	seg = vmem_btag_alloc(vmem, vmflag);
	if(unlikely(!seg)) {
		vmem->total_size -= size;
		vmem_btag_free(span);
		goto fail1;
	}

	seg->base = ret;
	seg->size = size;
	seg->span = span;
	seg->flags = VMEM_BTAG_SEGMENT | VMEM_BTAG_ALLOC;

	/* Insert the segment after the span. */
	list_add_after(&span->tag_link, &seg->tag_link);

	dprintf("vmem: imported span [0x%" PRIx64 ", 0x%" PRIx64 ") (vmem: %s, source: %s)\n",
		ret, ret + size, vmem->name, vmem->source->name);
	vmem->imported_size += size;
	return seg;
fail1:
	mutex_unlock(&vmem->lock);
	if(vmem->release) {
		vmem->release(ret, size);
	}
fail2:
	if(flags & VMEM_BTAG_XIMPORT) {
		vmem_xfree(vmem->source, ret, size);
	} else {
		vmem_free(vmem->source, ret, size);
	}
	mutex_lock(&vmem->lock);
	return NULL;
}

/** Unimport a span if it is no longer required.
 * @param vmem		Arena to unimport from.
 * @param span		Span to unimport. */
static void vmem_unimport(vmem_t *vmem, vmem_btag_t *span) {
	vmem_btag_t *seg;

	assert(span);
	assert(span->flags & VMEM_BTAG_IMPORT);

	/* Check whether the span still has allocated segments. If we're
	 * followed by a free segment covering the entire span we're OK to
	 * unimport. */
	seg = list_entry(span->tag_link.next, vmem_btag_t, tag_link);
	if(seg->flags & VMEM_BTAG_ALLOC || (seg->base != span->base && seg->size != span->size)) {
		return;
	}

	/* Free the segment. Do not free the span yet as we need information in
	 * it to unimport. */
	vmem_freelist_remove(vmem, seg);
	vmem_btag_free(seg);
	list_remove(&span->tag_link);

	vmem->total_size -= span->size;
	vmem->imported_size -= span->size;

	mutex_unlock(&vmem->lock);

	/* Call the release callback. */
	if(vmem->release) {
		vmem->release(span->base, span->size);
	}

	/* Free back to the source arena. */
	if(span->flags & VMEM_BTAG_XIMPORT) {
		vmem_xfree(vmem->source, span->base, span->size);
	} else {
		vmem_free(vmem->source, span->base, span->size);
	}

	mutex_lock(&vmem->lock);

	vmem_btag_free(span);
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
 * @todo		Implement the align and nocross constraints.
 *
 * @param vmem		Arena to allocate from.
 * @param size		Size of the segment to allocate.
 * @param align		Alignment of allocation.
 * @param nocross	Alignment boundary the allocation should not go across.
 * @param minaddr	Minimum start address of the allocation.
 * @param maxaddr	Highest end address of the allocation.
 * @param vmflag	Allocation flags.
 *
 * @return		Address of the allocation, or NULL on failure.
 */
vmem_resource_t vmem_xalloc(vmem_t *vmem, vmem_resource_t size, vmem_resource_t align,
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

	if(unlikely(align || nocross)) {
		fatal("TODO: Implement align and nocross constraints");
	}

	mutex_lock(&vmem->lock);

	/* Continuously loop until we can make the allocation. If MM_SLEEP is
	 * not set, this will break out once reclaiming cannot free any space
	 * in the arena. */
	while(true) {
		/* First try to find a free segment in the arena. */
		seg = vmem_find_segment(vmem, size, minaddr, maxaddr, vmflag);
		if(seg) {
			break;
		}

		/* If there is a source arena, try importing from it. Don't
		 * need to bother sleeping if we cannot import from the source:
		 * the allocation flags get passed down so waiting should take
		 * place at the arena at the end of the chain. */
		if(vmem->source) {
			seg = vmem_import(vmem, size, align, nocross, minaddr, maxaddr, vmflag);
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
		list_append(&vmem->alloc_hash[hash], &seg->af_link);

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
		tag = list_entry(iter, vmem_btag_t, af_link);

		assert(tag->flags & VMEM_BTAG_ALLOC);
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

		tag->flags &= ~VMEM_BTAG_ALLOC;

		vmem->used_size -= tag->size;
		vmem->used_segs--;

		/* Coalesce adjacent free segments. */
		if(tag->tag_link.next != &vmem->btags) {
			exist = list_entry(tag->tag_link.next, vmem_btag_t, tag_link);
			if(exist->flags == tag->flags) {
				assert((tag->base + tag->size) == exist->base);
				tag->size += exist->size;
				vmem_freelist_remove(vmem, exist);
				vmem_btag_free(exist);
			}
		}

		/* Can't be the list header because there should be a span before. */
		assert(tag->tag_link.prev != &vmem->btags);

		exist = list_entry(tag->tag_link.prev, vmem_btag_t, tag_link);
		if(exist->flags == tag->flags) {
			assert((exist->base + exist->size) == tag->base);
			tag->base = exist->base;
			tag->size += exist->size;
			vmem_freelist_remove(vmem, exist);
			vmem_btag_free(exist);
		}

		vmem_freelist_insert(vmem, tag);

		/* Check if the span can be unimported. */
		if(vmem->source && tag->span->flags & VMEM_BTAG_IMPORT) {
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

	return vmem_xalloc(vmem, size, 0, 0, 0, 0, vmflag);
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

/** Check if a vmem arena contains a span.
 * @param vmem		Arena to check in.
 * @param base		Start of span to check.
 * @param size		End of the span.
 * @return		Whether the span overlaps an existing span. */
static bool vmem_contains(vmem_t *vmem, vmem_resource_t base, vmem_resource_t end) {
	vmem_resource_t btend;
	vmem_btag_t *btag;

	LIST_FOREACH(&vmem->btags, iter) {
		btag = list_entry(iter, vmem_btag_t, tag_link);

		btend = btag->base + btag->size;
		if((btag->flags & VMEM_BTAG_TYPE) != VMEM_BTAG_SPAN) {
			continue;
		} else if(base >= btag->base && base < btend) {
			return true;
		} else if(end > btag->base && end <= btend) {
			return true;
		}
	}

	return false;
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
	if(vmem_contains(vmem, base, base + size)) {
		fatal("Tried to add overlapping span [0x%" PRIx64 ", 0x%" PRIx64 ") to %p",
			base, base + size, vmem);
	}

	/* Create the span itself. */
	span = vmem_add_internal(vmem, base, size, 0, vmflag);
	if(!span) {
		mutex_unlock(&vmem->lock);
		return false;
	}

	/* Create a free segment. */
	seg = vmem_btag_alloc(vmem, vmflag);
	if(!seg) {
		vmem_btag_free(span);
		mutex_unlock(&vmem->lock);
		return false;
	}

	seg->base = base;
	seg->size = size;
	seg->span = span;
	seg->flags = VMEM_BTAG_SEGMENT;

	/* Place the segment after the span and add it to the freelists. */
	list_add_after(&span->tag_link, &seg->tag_link);
	vmem_freelist_insert(vmem, seg);

	dprintf("vmem: added span [0x%" PRIx64 ", 0x%" PRIx64 ") to %p(%s)\n",
	        base, base + size, vmem, vmem->name);
	mutex_unlock(&vmem->lock);
	return true;
}

/** Initialise a vmem arena.
 * @param vmem		Arena to initialise.
 * @param name		Name of the arena for debugging purposes.
 * @param quantum	Allocation granularity.
 * @param type		Type of the resource the arena is allocating, or 0.
 * @param flags		Behaviour flags for the arena.
 * @param source	Arena backing this arena.
 * @param import	Callback function for importing from the source.
 * @param release	Callback function for releasing to the source.
 * @param qcache_max	Maximum size to cache.
 * @param base		Start of the initial span.
 * @param size		Size of the initial span.
 * @param vmflag	Allocation flags.
 * @return		Whether the arena was successfully initialised. */
bool vmem_early_create(vmem_t *vmem, const char *name, size_t quantum, uint32_t type, int flags,
                       vmem_t *source, vmem_import_t import, vmem_release_t release, size_t qcache_max,
                       vmem_resource_t base, vmem_resource_t size, int vmflag) {
	char qcname[SLAB_NAME_MAX];
	size_t i;

	assert(vmem);
	assert(quantum);
	assert(IS_POW2(quantum));
	assert(!(base % quantum));
	assert(!(size % quantum));
	assert(!(qcache_max % quantum));
	assert(source != vmem);

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
	vmem->flags = flags;
	vmem->free_map = 0;
	vmem->alloc_hash = vmem->initial_hash;
	vmem->alloc_hash_size = VMEM_HASH_INITIAL;
	vmem->rehash_requested = false;
	vmem->source = source;
	vmem->import = import;
	vmem->release = release;
	vmem->total_size = 0;
	vmem->used_size = 0;
	vmem->imported_size = 0;
	vmem->used_segs = 0;
	vmem->alloc_count = 0;
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
 * @param name		Name of the arena for debugging purposes.
 * @param base		Start of the initial span.
 * @param size		Size of the initial span.
 * @param quantum	Allocation granularity.
 * @param import	Callback function for importing from the source.
 * @param release	Callback function for releasing to the source.
 * @param source	Arena backing this arena.
 * @param qcache_max	Maximum size to cache.
 * @param type		Type of the resource the arena is allocating, or 0.
 * @param vmflag	Allocation flags.
 * @return		Pointer to arena on success, NULL on failure. */
vmem_t *vmem_create(const char *name, size_t quantum, uint32_t type, int flags, vmem_t *source,
                    vmem_import_t import, vmem_release_t release, size_t qcache_max,
                    vmem_resource_t base, vmem_resource_t size, int vmflag) {
	vmem_t *vmem;

	vmem = kmalloc(sizeof(vmem_t), vmflag & MM_FLAG_MASK);
	if(!vmem) {
		return NULL;
	}

	if(!vmem_early_create(vmem, name, quantum, type, flags, source, import, release,
	                      qcache_max, base, size, vmflag)) {
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

		if(strcmp(vmem->name, name) == 0) {
			return vmem;
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

		kprintf(LOG_NONE, "%*s%-*s %-4u %-16" PRIu64 " %-16" PRIu64 " %zu\n",
		        indent, "", VMEM_NAME_MAX - indent, vmem->name, vmem->type,
		        vmem->total_size, vmem->used_size, vmem->alloc_count);
		vmem_dump_list(&vmem->children, indent + 2);
	}
}

/** KDBG vmem information command.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_vmem(int argc, char **argv) {
	char *name = NULL;
	vmem_btag_t *btag;
	bool dump = false;
	unative_t addr;
	vmem_t *vmem;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [[--dump] <arena>]\n\n", argv[0]);

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

	if(strcmp(argv[1], "--dump") == 0) {
		dump = true;
	}

	if(kdbg_parse_expression(argv[(dump) ? 2 : 1], &addr, &name) != KDBG_OK) {
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
		kprintf(LOG_NONE, "Source: %p(%s)  Imported: %" PRIu64 "\n",
		        vmem->source, vmem->source->name, vmem->imported_size);
	}

	/* Print out a span/segment list if requested. */
	if(dump) {
		kprintf(LOG_NONE, "\n");
		kprintf(LOG_NONE, "Base                 End                  Type\n");
		kprintf(LOG_NONE, "====                 ===                  ====\n");
		LIST_FOREACH(&vmem->btags, iter) {
			btag = list_entry(iter, vmem_btag_t, tag_link);

			if((btag->flags & VMEM_BTAG_TYPE) == VMEM_BTAG_SPAN) {
				kprintf(LOG_NONE, "0x%016" PRIx64 "   0x%016" PRIx64 "   Span%s\n",
				            btag->base, btag->base + btag->size,
				            (btag->flags & VMEM_BTAG_IMPORT) ? " (Imported)" : "");
			} else {
				kprintf(LOG_NONE, "  0x%016" PRIx64 "   0x%016" PRIx64 " Segment %s\n",
				            btag->base, btag->base + btag->size,
				            (btag->flags & VMEM_BTAG_ALLOC) ? "(Allocated)" : "(Free)");
			}
		}
	}

	return KDBG_OK;
}

/** Add the initial tags to the boundary tag list. */
__init_text void vmem_early_init(void) {
	size_t i;

	for(i = 0; i < VMEM_BOOT_TAG_COUNT; i++) {
		list_init(&vmem_boot_tags[i].tag_link);
		list_init(&vmem_boot_tags[i].af_link);
		list_append(&vmem_btags, &vmem_boot_tags[i].tag_link);
		vmem_btag_count++;
	}
}

/** Create the boundary tag arena. */
__init_text void vmem_init(void) {
	/* Create the boundary tag arena. */
	vmem_early_create(&vmem_btag_arena, "vmem_btag_arena", PAGE_SIZE, 0, VMEM_REFILL,
	                  &kheap_raw_arena, kheap_anon_import, kheap_anon_release, 0, 0,
	                  0, MM_FATAL);
}

/** Start the periodic maintenance timer. */
__init_text void vmem_late_init(void) {
	timer_init(&vmem_maintenance_timer, vmem_maintenance, NULL, TIMER_THREAD);
	timer_start(&vmem_maintenance_timer, VMEM_PERIODIC_INTERVAL, TIMER_PERIODIC);
}
