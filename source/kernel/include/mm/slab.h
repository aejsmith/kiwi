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
 * @brief		Slab allocator.
 */

#ifndef __MM_SLAB_H
#define __MM_SLAB_H

#include <arch/page.h>

#include <lib/atomic.h>
#include <lib/list.h>

#include <mm/flags.h>

#include <sync/mutex.h>

struct vmem;
struct slab_magazine;
struct slab_bufctl;

/** Allocator limitations/settings. */
#define SLAB_NAME_MAX		25		/**< Maximum slab cache name length. */
#define SLAB_MAGAZINE_SIZE	16		/**< Initial magazine size (resizing currently not supported). */
#define SLAB_HASH_SIZE		64		/**< Allocation hash table size. */
#define SLAB_ALIGN_MIN		8		/**< Minimum alignment. */
#define SLAB_LARGE_FRACTION	8		/**< Minimum fraction of the source quantum for large objects. */
#define SLAB_WASTE_FRACTION	8		/**< Maximum fraction of a slab that should be wasted. */

/** Slab constructor callback function. */
typedef void (*slab_ctor_t)(void *obj, void *data);

/** Slab destructor callback function. */
typedef void (*slab_dtor_t)(void *obj, void *data);

/** Slab CPU cache structure. */
typedef struct slab_cpu_cache {
	struct slab_magazine *loaded;		/**< Current (loaded) magazine. */
	struct slab_magazine *previous;		/**< Previous magazine. */
} __cacheline_aligned slab_cpu_cache_t;

/** Slab cache structure. */
typedef struct slab_cache {
	/** Magazine depot structures. */
	mutex_t depot_lock;			/**< Magazine depot lock. */
	list_t magazine_full;			/**< List of full magazines. */
	list_t magazine_empty;			/**< List of empty magazines. */

	/** Statistics. */
#if CONFIG_SLAB_STATS
	atomic_t alloc_total;			/**< Total number of allocations that have been made. */
	atomic_t alloc_current;			/**< Number of currently allocated objects. */
#endif
	size_t slab_count;			/**< Number of allocated slabs. */

	/** Slab lists/cache colouring settings. */
	mutex_t slab_lock;			/**< Lock to protect slab lists. */
	list_t slab_partial;			/**< List of partially allocated slabs. */
	list_t slab_full;			/**< List of fully allocated slabs. */
	size_t colour_next;			/**< Next cache colour. */
	size_t colour_max;			/**< Maximum cache colour. */

	/** Allocation hash table for no-touch caches. */
	struct slab_bufctl *bufctl_hash[SLAB_HASH_SIZE];

	/** Cache settings. */
	int flags;				/**< Cache behaviour flags. */
	size_t slab_size;			/**< Size of a slab. */
	size_t obj_size;			/**< Size of an object. */
	size_t obj_count;			/**< Number of objects per slab. */
	size_t align;				/**< Required alignment of each object. */
	struct vmem *source;			/**< Vmem arena to use for memory allocation. */

	/** Callback functions. */
	slab_ctor_t ctor;			/**< Object constructor function. */
	slab_dtor_t dtor;			/**< Object destructor function. */
	void *data;				/**< Data to pass to helper functions. */
	int priority;				/**< Reclaim priority. */

	/** Debugging information. */
	list_t header;				/**< List to slab cache list. */
	char name[SLAB_NAME_MAX];		/**< Name of cache. */

	/** Per-CPU caches.
	 * @note		These will be allocated onto the end of the
	 *			structure as necessary. They will be correctly
	 *			aligned to a cacheline boundary to prevent
	 *			sharing between CPUs. */
	slab_cpu_cache_t cpu_caches[];
} __cacheline_aligned slab_cache_t;

/** Slab cache flags. */
#define SLAB_CACHE_NOMAG	(1<<0)		/**< Disable the magazine layer. */
#define SLAB_CACHE_NOTOUCH	(1<<1)		/**< Always store metadata outside of allocated memory. */
#define SLAB_CACHE_QCACHE	(1<<2)		/**< Cache is serving as a quantum cache for its source. */

extern void *slab_cache_alloc(slab_cache_t *cache, int kmflag);
extern void slab_cache_free(slab_cache_t *cache, void *obj);

/** Expands to size and align argument for slab_cache_create(). */
#define SLAB_SIZE_ALIGN(t)	sizeof(t), __alignof__(t)

extern slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align,
                                       slab_ctor_t ctor, slab_dtor_t dtor,
                                       void *data, struct vmem *source, int flags,
                                       int kmflag);
extern void slab_cache_destroy(slab_cache_t *cache);

extern int kdbg_cmd_slab(int argc, char **argv);

extern void slab_init(void);

#endif /* __MM_SLAB_H */
