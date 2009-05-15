/* Kiwi slab allocator
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Slab allocator.
 */

#ifndef __MM_SLAB_H
#define __MM_SLAB_H

#include <arch/page.h>

#include <mm/flags.h>

#include <sync/mutex.h>

#include <types/atomic.h>
#include <types/list.h>

struct vmem;
struct slab_cpu_cache;
struct slab_bufctl;

/** Allocator limitations/settings. */
#define SLAB_NAME_MAX		25		/**< Maximum slab cache name length. */
#define SLAB_MAGAZINE_SIZE	8		/**< Initial magazine size (resizing currently not supported). */
#define SLAB_HASH_SIZE		32		/**< Allocation hash table size. */
#define SLAB_ALIGN_MIN		8		/**< Minimum alignment. */
#define SLAB_LARGE_FRACTION	8		/**< Minimum fraction of the source quantum for large objects. */
#define SLAB_WASTE_FRACTION	8		/**< Maximum fraction of a slab that should be wasted. */

/** Slab constructor callback function. */
typedef int (*slab_ctor_t)(void *obj, void *data, int kmflag);

/** Slab destructor callback function. */
typedef void (*slab_dtor_t)(void *obj, void *data);

/** Slab cache structure. */
typedef struct slab_cache {
	/** Magazine layer structures. */
	struct slab_cpu_cache *cpu_caches;	/**< Array of magazine caches for all CPUs. */
	mutex_t depot_lock;			/**< Magazine depot lock. */
	list_t magazine_full;			/**< List of full magazines. */
	list_t magazine_empty;			/**< List of empty magazines. */

	/** Statistics. */
	atomic_t alloc_count;			/**< Total number of allocations that have been made. */

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

	/** Things related to slab allocation/destruction. */
	slab_ctor_t ctor;			/**< Object constructor function. */
	slab_dtor_t dtor;			/**< Object destructor function. */
	void *data;				/**< Data to pass to helper functions. */
	struct vmem *source;			/**< Vmem arena to use for memory allocation. */

	/** Debugging information. */
	list_t header;				/**< List to slab cache list. */
	char name[SLAB_NAME_MAX];		/**< Name of cache. */
} slab_cache_t;

/** Slab cache flags. */
#define SLAB_CACHE_NOMAG	(1<<0)		/**< Disable the magazine layer. */
#define SLAB_CACHE_LATEMAG	(1<<1)		/**< Magazine layer should be enabled once CPU count is known. */
#define SLAB_CACHE_NOTOUCH	(1<<2)		/**< Always store metadata outside of allocated memory. */
#define SLAB_CACHE_QCACHE	(1<<3)		/**< Cache is serving as a quantum cache for its source. */

extern void *slab_cache_alloc(slab_cache_t *cache, int kmflag);
extern void slab_cache_free(slab_cache_t *cache, void *obj);

extern slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align,
                                       slab_ctor_t ctor, slab_dtor_t dtor, void *data,
                                       struct vmem *source, int flags, int kmflag);
extern void slab_cache_destroy(slab_cache_t *cache);

extern bool slab_reclaim(void);
extern void slab_enable_cpu_cache(void);
extern void slab_init(void);

extern int kdbg_cmd_slab(int argc, char **argv);

#endif /* __MM_SLAB_H */
