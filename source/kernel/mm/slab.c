/*
 * Copyright (C) 2009-2010 Alex Smith
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
 *
 * Reference:
 * - The slab allocator: An object-caching kernel memory allocator
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.29.4759
 * - Magazines and Vmem: Extending the Slab Allocator to Many CPUs and
 *   Arbitrary Resources
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.6.8388
 *
 * This implementation uses the magazine layer described in the second of the
 * above papers for good multiprocessor scalability. The only difference is
 * that we do not leave empty slabs lying around - when a slab becomes empty,
 * it is freed immediately.
 *
 * @todo		Dynamic magazine resizing.
 * @todo		Allocation hash table resizing.
 * @todo		We should align the cache structures on a cacheline
 *			boundary.
 */

#include <cpu/cpu.h>

#include <lib/hash.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <fatal.h>
#include <kdbg.h>
#include <lrm.h>
#include <status.h>
#include <vmem.h>

#if CONFIG_SLAB_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

struct slab;

/** Slab magazine structure. */
typedef struct slab_magazine {
	/** Array of objects in the magazine. */
	void *objects[SLAB_MAGAZINE_SIZE];
	size_t rounds;			/**< Number of rounds currently in the magazine. */

	list_t header;			/**< Link to depot lists. */
} slab_magazine_t;

/** Slab CPU cache structure. */
typedef struct slab_cpu_cache {
	mutex_t lock;			/**< CPU cache lock. */
	slab_magazine_t *loaded;	/**< Current (loaded) magazine. */
	slab_magazine_t *previous;	/**< Previous magazine. */
} slab_cpu_cache_t;

/** Slab buffer control structure. The order of this structure is important:
 *  the pointer MUST be first, as it is the only member that exists for
 *  small-object caches. */
typedef struct slab_bufctl {
	struct slab_bufctl *next;	/**< Address of next buffer. */

	struct slab *parent;		/**< Parent slab structure. */
	void *object;			/**< Pointer to actual object. */
} slab_bufctl_t;

/** Slab structure. */
typedef struct slab {
	list_t header;			/**< Link to appropriate slab list in cache. */

	void *base;			/**< Base address of allocation. */
	size_t refcount;		/**< Reference count. */
	slab_bufctl_t *free;		/**< List of free buffers. */
	size_t colour;			/**< Colour of the slab. */
	slab_cache_t *parent;		/**< Cache containing the slab. */
} slab_t;

/** Reclaim priorities to use for caches. */
#define SLAB_DEFAULT_PRIORITY		0
#define SLAB_QCACHE_PRIORITY		1
#define SLAB_METADATA_PRIORITY		2
#define SLAB_MAG_PRIORITY		3

/** Internally-used caches. */
static slab_cache_t slab_cache_cache;	/**< Cache for allocation of new slab caches. */
static slab_cache_t slab_bufctl_cache;	/**< Cache for buffer control structures. */
static slab_cache_t slab_slab_cache;	/**< Cache for slab structures. */
static slab_cache_t slab_mag_cache;	/**< Cache for magazine structures. */

/** Vmem arena to back the internal caches. */
static vmem_t slab_metadata_arena;

/** List of all slab caches. */
static LIST_DECLARE(slab_caches);
static MUTEX_DECLARE(slab_caches_lock, 0);

/** Whether the allocator is fully initialised. */
static bool slab_inited = false;

/** Work out the optimal slab size for a cache.
 * @todo		Better implementation.
 * @param cache		Cache to work out for.
 * @return		Slab size to use. */
static inline size_t slab_get_slabsize(slab_cache_t *cache) {
	size_t size;

	if(cache->flags & SLAB_CACHE_QCACHE) {
		return VMEM_QCACHE_SSIZE(cache->source->qcache_max);
	} else if(cache->obj_size < (cache->source->quantum / SLAB_LARGE_FRACTION)) {
		return cache->source->quantum;
	}

	/* Don't need to worry about space taken up by metadata here: the
	 * no-touch flag will always be enabled for objects of this size (see
	 * above check). */
	size = ROUND_UP(cache->obj_size, cache->source->quantum);
	while((size % cache->obj_size) > (size / SLAB_WASTE_FRACTION)) {
		size += cache->source->quantum;
	}

	return size;
}

/** Destroy a slab.
 * @param cache		Cache to destroy in.
 * @param slab		Slab to destroy. */
static void slab_destroy(slab_cache_t *cache, slab_t *slab) {
	unative_t addr = (unative_t)slab->base;
	slab_bufctl_t *bufctl;

	if(unlikely(slab->refcount != 0)) {
		fatal("Slab (%s) has %zu references while destructing",
		      cache->name, slab->refcount);
	}

	list_remove(&slab->header);

	/* Destroy all buffer control structures and the slab structure if
	 * stored externally. */
	if(cache->flags & SLAB_CACHE_NOTOUCH) {
		while(slab->free != NULL) {
			bufctl = slab->free;
			slab->free = bufctl->next;

			slab_cache_free(&slab_bufctl_cache, bufctl);
		}

		slab_cache_free(&slab_slab_cache, slab);
	}

	cache->slab_count--;
	vmem_free(cache->source, addr, cache->slab_size);
}

/** Allocate a new slab and divide it up into objects.
 * @param cache		Cache to allocate from.
 * @param kmflag	Allocation behaviour flags.
 * @return		Pointer to slab structure. */
static inline slab_t *slab_create(slab_cache_t *cache, int kmflag) {
	slab_bufctl_t *bufctl, *prev = NULL;
	unative_t addr;
	slab_t *slab;
	size_t i;

	/* Drop slab lock while creating as a reclaim may occur that wants to
	 * free to this cache. */
	mutex_unlock(&cache->slab_lock);

	/* Allocate a new slab. */
	addr = vmem_alloc(cache->source, cache->slab_size, (kmflag & MM_FLAG_MASK) & ~MM_FATAL);
	if(unlikely(addr == 0)) {
		/* Handle MM_FATAL ourselves rather than converting to vmflag
		 * so that we get a more accurate error message. */
		if(kmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation on object cache %p(%s) (1)",
			      cache, cache->name);
		}
		mutex_lock(&cache->slab_lock);
		return NULL;
	}

	mutex_lock(&cache->slab_lock);

	/* Create the slab structure for the slab. */
	if(cache->flags & SLAB_CACHE_NOTOUCH) {
		slab = slab_cache_alloc(&slab_slab_cache, kmflag & ~MM_FATAL);
		if(unlikely(slab == NULL)) {
			/* Same as above. */
			if(kmflag & MM_FATAL) {
				fatal("Could not perform mandatory allocation on object cache %p(%s) (2)",
				      cache, cache->name);
			}

			vmem_free(cache->source, addr, cache->slab_size);
			return NULL;
		}
	} else {
		slab = (slab_t *)((addr + cache->slab_size) - sizeof(slab_t));
	}

	cache->slab_count++;

	list_init(&slab->header);
	slab->base = (void *)addr;
	slab->refcount = 0;
	slab->free = NULL;
	slab->colour = cache->colour_next;
	slab->parent = cache;

	/* Divide the buffer up into unconstructed, free objects. */
	for(i = 0; i < cache->obj_count; i++) {
		if(cache->flags & SLAB_CACHE_NOTOUCH) {
			bufctl = slab_cache_alloc(&slab_bufctl_cache, kmflag & ~MM_FATAL);
			if(unlikely(bufctl == NULL)) {
				/* Same as above. */
				if(kmflag & MM_FATAL) {
					fatal("Could not perform mandatory allocation on object cache %p(%s) (3)",
					      cache, cache->name);
				}

				slab_destroy(cache, slab);
				return NULL;
			}

			bufctl->parent = slab;
			bufctl->object = (void *)(addr + slab->colour + (i * cache->obj_size));
		} else {
			bufctl = (slab_bufctl_t *)(addr + slab->colour + (i * cache->obj_size));
		}

		/* Add to the free list. */
		bufctl->next = NULL;
		if(prev == NULL) {
			slab->free = bufctl;
		} else {
			prev->next = bufctl;
		}
		prev = bufctl;
	}

	/* Success - update the cache colour and return. Do not add to any
	 * slab lists - the caller will do so. */
	cache->colour_next += cache->align;
	if(cache->colour_next > cache->colour_max) {
		cache->colour_next = 0;
	}

	return slab;
}

/** Destruct an object and free it to the slab layer.
 * @param cache		Cache to free to.
 * @param obj		Object to free. */
static inline void slab_obj_free(slab_cache_t *cache, void *obj) {
	slab_bufctl_t *bufctl, *prev = NULL;
	uint32_t hash;
	slab_t *slab;

	mutex_lock(&cache->slab_lock);

	/* Find the buffer control structure. For no-touch caches, look it up
	 * on the allocation hash table. Otherwise, we use the start of the
	 * buffer as the structure. */
	if(cache->flags & SLAB_CACHE_NOTOUCH) {
		hash = hash_int_hash((key_t)((ptr_t)obj)) % SLAB_HASH_SIZE;
		for(bufctl = cache->bufctl_hash[hash]; bufctl != NULL; bufctl = bufctl->next) {
			if(bufctl->object == obj) {
				break;
			}
			prev = bufctl;
		}

		if(unlikely(bufctl == NULL)) {
			fatal("Free(%s): object %p not found in hash table", cache->name, obj);
		}

		slab = bufctl->parent;
		assert(slab->parent == cache);

		/* Take the object off the hash chain. */
		if(prev != NULL) {
			prev->next = bufctl->next;
		} else {
			cache->bufctl_hash[hash] = bufctl->next;
		}
	} else {
		bufctl = (slab_bufctl_t *)obj;

		/* Find the slab corresponding to the object. The structure
		 * will be at the end of the slab. */
		slab = (slab_t *)(ROUND_DOWN((ptr_t)obj, cache->slab_size) + (cache->slab_size - sizeof(slab_t)));
		if(unlikely(slab->parent != cache)) {
			fatal("Free(%s): slab structure for %p invalid (%p)", cache->name, obj, slab->parent);
		}
	}

	/* Destruct the object if necessary. */
	if(cache->dtor) {
		cache->dtor(obj, cache->data);
	}

	assert(slab->refcount);

	/* Return the object to the slab's free list. */
	bufctl->next = slab->free;
	slab->free = bufctl;

	if(--slab->refcount == 0) {
		/* Slab empty, destroy it. */
		slab_destroy(cache, slab);
	} else if((slab->refcount + 1) == cache->obj_count) {
		/* Take from the full list and move to the partial list. */
		list_append(&cache->slab_partial, &slab->header);
	}

	mutex_unlock(&cache->slab_lock);
}

/** Allocate an object from the slab layer and construct it.
 * @param cache		Cache to allocate from.
 * @param kmflag	Allocation behaviour flags.
 * @return		Pointer to allocated object, or NULL on failure. */
static inline void *slab_obj_alloc(slab_cache_t *cache, int kmflag) {
	slab_bufctl_t *bufctl;
	uint32_t hash;
	slab_t *slab;
	void *obj;

	mutex_lock(&cache->slab_lock);

	/* If there is a slab in the partial list, take it. */
	if(!list_empty(&cache->slab_partial)) {
		slab = list_entry(cache->slab_partial.next, slab_t, header);
	} else {
		/* No slabs with free objects available - allocate a new
		 * slab. */
		slab = slab_create(cache, kmflag);
		if(unlikely(slab == NULL)) {
			mutex_unlock(&cache->slab_lock);
			return NULL;
		}
	}

	assert(slab->free);

	/* Take an object from the slab. If the metadata is stored externally,
	 * then the object address is contained in the object field of the
	 * bufctl structure. Otherwise, the object address is the same as the
	 * structure address. */
	bufctl = slab->free;
	slab->free = bufctl->next;
	slab->refcount++;

	obj = (cache->flags & SLAB_CACHE_NOTOUCH) ? bufctl->object : (void *)bufctl;

	/* Place the allocation on the allocation hash table if required. */
	if(cache->flags & SLAB_CACHE_NOTOUCH) {
		hash = hash_int_hash((key_t)((ptr_t)obj)) % SLAB_HASH_SIZE;
		bufctl->next = cache->bufctl_hash[hash];
		cache->bufctl_hash[hash] = bufctl;
	}

	/* Check if a list move is required. */
	if(slab->refcount == cache->obj_count) {
		list_append(&cache->slab_full, &slab->header);
	} else {
		list_append(&cache->slab_partial, &slab->header);
	}

	/* Construct the object and return it. Unlock the cache before calling
	 * the constructor as it may cause a reclaim. */
	mutex_unlock(&cache->slab_lock);
	if(cache->ctor) {
		cache->ctor(obj, cache->data);
	}
	return obj;
}

/** Get a full magazine from a cache's depot.
 * @param cache		Cache to get from.
 * @return		Pointer to magazine on success, NULL on failure. */
static inline slab_magazine_t *slab_magazine_get_full(slab_cache_t *cache) {
	slab_magazine_t *mag = NULL;

	mutex_lock(&cache->depot_lock);

	if(!list_empty(&cache->magazine_full)) {
		mag = list_entry(cache->magazine_full.next, slab_magazine_t, header);
		list_remove(&mag->header);
		assert(mag->rounds == SLAB_MAGAZINE_SIZE);
	}

	mutex_unlock(&cache->depot_lock);
	return mag;
}

/** Return a full magazine to the depot.
 * @param cache		Cache to return to.
 * @param mag		Magazine to return. */
static inline void slab_magazine_put_full(slab_cache_t *cache, slab_magazine_t *mag) {
	assert(mag->rounds == SLAB_MAGAZINE_SIZE);

	mutex_lock(&cache->depot_lock);
	list_prepend(&cache->magazine_full, &mag->header);
	mutex_unlock(&cache->depot_lock);
}

/** Get an empty magazine from a cache's depot.
 * @param cache		Cache to get from.
 * @return		Pointer to magazine on success, NULL on failure. */
static inline slab_magazine_t *slab_magazine_get_empty(slab_cache_t *cache) {
	slab_magazine_t *mag = NULL;
	int level;

	mutex_lock(&cache->depot_lock);

	if(!list_empty(&cache->magazine_empty)) {
		mag = list_entry(cache->magazine_empty.next, slab_magazine_t, header);
		list_remove(&mag->header);
		assert(!mag->rounds);
	} else {
		/* Do not attempt to allocate a magazine if low on memory. */
		level = lrm_level(RESOURCE_TYPE_MEMORY | RESOURCE_TYPE_KASPACE);
		if(likely(level == RESOURCE_LEVEL_OK)) {
			mag = slab_cache_alloc(&slab_mag_cache, 0);
			if(mag != NULL) {
				list_init(&mag->header);
				mag->rounds = 0;
			}
		}
	}

	mutex_unlock(&cache->depot_lock);
	return mag;
}

/** Return an empty magazine to the depot.
 * @param cache		Cache to return to.
 * @param mag		Magazine to return. */
static inline void slab_magazine_put_empty(slab_cache_t *cache, slab_magazine_t *mag) {
	assert(!mag->rounds);

	mutex_lock(&cache->depot_lock);
	list_prepend(&cache->magazine_empty, &mag->header);
	mutex_unlock(&cache->depot_lock);
}

/** Destroy a magazine.
 * @param cache		Cache the magazine belongs to.
 * @param mag		Magazine to destroy. */
static inline void slab_magazine_destroy(slab_cache_t *cache, slab_magazine_t *mag) {
	size_t i;

	/* Free all rounds within the magazine, if any. */
	for(i = 0; i < mag->rounds; i++) {
		slab_obj_free(cache, mag->objects[i]);
	}

	list_remove(&mag->header);
	slab_cache_free(&slab_mag_cache, mag);
}

/** Move current magazine to previous and load a new magazine.
 * @param cc		CPU cache to reload.
 * @param mag		New magazine. */
static inline void slab_cpu_reload(slab_cpu_cache_t *cc, slab_magazine_t *mag) {
	cc->previous = cc->loaded;
	cc->loaded = mag;
}

/** Allocate an object from the magazine layer.
 * @param cache		Cache to allocate from.
 * @return		Pointer to object on success, NULL on failure. */
static inline void *slab_cpu_obj_alloc(slab_cache_t *cache) {
	slab_cpu_cache_t *cc = &cache->cpu_caches[curr_cpu->id];
	slab_magazine_t *mag;
	void *ret = NULL;

	mutex_lock(&cc->lock);

	/* Check if we have a magazine to allocate from. */
	if(likely(cc->loaded)) {
		if(cc->loaded->rounds) {
			ret = cc->loaded->objects[--cc->loaded->rounds];
			goto out;
		} else if(cc->previous && cc->previous->rounds) {
			/* Previous has rounds, exchange loaded with previous
			 * and allocate from it. */
			slab_cpu_reload(cc, cc->previous);
			ret = cc->loaded->objects[--cc->loaded->rounds];
			goto out;
		}
	}

	/* Try to get a full magazine from the depot. */
	mag = slab_magazine_get_full(cache);
	if(likely(mag != NULL)) {
		/* Return previous to the depot. */
		if(cc->previous) {
			slab_magazine_put_empty(cache, cc->previous);
		}

		slab_cpu_reload(cc, mag);
		ret = cc->loaded->objects[--cc->loaded->rounds];
	}
out:
	mutex_unlock(&cc->lock);
	return ret;
}

/** Free an object to the magazine layer.
 * @param cache		Cache to free to.
 * @param obj		Object to free.
 * @return		True if succeeded, false if not. */
static inline bool slab_cpu_obj_free(slab_cache_t *cache, void *obj) {
	slab_cpu_cache_t *cc = &cache->cpu_caches[curr_cpu->id];
	slab_magazine_t *mag;

	mutex_lock(&cc->lock);

	/* If the loaded magazine has spare slots, just put the object there
	 * and return. */
	if(likely(cc->loaded != NULL)) {
		if(cc->loaded->rounds < SLAB_MAGAZINE_SIZE) {
			cc->loaded->objects[cc->loaded->rounds++] = obj;
			mutex_unlock(&cc->lock);
			return true;
		} else if(cc->previous && cc->previous->rounds < SLAB_MAGAZINE_SIZE) {
			/* Previous has spare slots, exchange them and insert
			 * the object. */
			slab_cpu_reload(cc, cc->previous);
			cc->loaded->objects[cc->loaded->rounds++] = obj;
			mutex_unlock(&cc->lock);
			return true;
		}
	}

	/* Get a new empty magazine. */
	mag = slab_magazine_get_empty(cache);
	if(unlikely(mag == NULL)) {
		mutex_unlock(&cc->lock);
		return false;
	}

	/* Load the new magazine, and free the previous. */
	if(likely(cc->previous)) {
		slab_magazine_put_full(cache, cc->previous);
	}
	slab_cpu_reload(cc, mag);

	cc->loaded->objects[cc->loaded->rounds++] = obj;
	mutex_unlock(&cc->lock);
	return true;
}

/** Create the CPU cache for a slab cache.
 * @param cache		Cache to create for.
 * @param kmflag	Allocation flags.
 * @return		Whether the cache was created. */
static bool slab_cpu_cache_init(slab_cache_t *cache, int kmflag) {
	size_t i;

	assert(slab_inited);

	cache->cpu_caches = kcalloc(cpu_id_max + 1, sizeof(slab_cpu_cache_t), kmflag);
	if(!cache->cpu_caches) {
		return false;
	}

	/* Initialise the cache structures. */
	for(i = 0; i <= cpu_id_max; i++) {
		mutex_init(&cache->cpu_caches[i].lock, "cpu_cache_lock", 0);
	}

	return true;
}

/** Reclaim memory from a slab cache.
 * @todo		Should we reclaim partial magazines too, somehow?
 * @param cache		Cache to reclaim from.
 * @param force		Whether to force reclaim of everything.
 * @return		Whether any slabs were freed. */
static inline bool slab_cache_reclaim(slab_cache_t *cache, bool force) {
	bool ret = false;
	size_t count;

	kprintf(LOG_DEBUG, "slab: reclaiming from cache %p(%s)...\n", cache, cache->name);

	mutex_lock(&cache->depot_lock);

	/* Destroy empty magazines. */
	LIST_FOREACH_SAFE(&cache->magazine_empty, iter) {
		slab_magazine_destroy(cache, list_entry(iter, slab_magazine_t, header));
	}

	/* Get the slab count before destroying. */
	if(!(count = cache->slab_count)) {
		mutex_unlock(&cache->depot_lock);
		return false;
	}

	/* Destroy full magazines until the slab count decreases. */
	LIST_FOREACH_SAFE(&cache->magazine_full, iter) {
		slab_magazine_destroy(cache, list_entry(iter, slab_magazine_t, header));
		if(cache->slab_count < count) {
			ret = true;
			if(!force) {
				break;
			}
		}
	}

	mutex_unlock(&cache->depot_lock);
	return ret;
}

/** Allocate a constructed object from a slab cache.
 * @param cache		Cache to allocate from.
 * @param kmflag	Allocation behaviour flags.
 * @return		Pointer to allocated object or NULL if unable to
 *			allocate.  */
void *slab_cache_alloc(slab_cache_t *cache, int kmflag) {
	void *ret;

	assert(cache);

	if(!(cache->flags & SLAB_CACHE_NOMAG)) {
		ret = slab_cpu_obj_alloc(cache);
		if(likely(ret != NULL)) {
#if CONFIG_SLAB_STATS
			atomic_inc(&cache->alloc_total);
			atomic_inc(&cache->alloc_current);
#endif
			dprintf("slab: allocated %p from cache %p(%s) (magazine)\n", ret, cache, cache->name);
			return ret;
		}
	}

	/* Cannot allocate from magazine layer, allocate from slab layer. */
	ret = slab_obj_alloc(cache, kmflag);
	if(likely(ret != NULL)) {
#if CONFIG_SLAB_STATS
		atomic_inc(&cache->alloc_total);
		atomic_inc(&cache->alloc_current);
#endif
		dprintf("slab: allocated %p from cache %p(%s) (slab)\n", ret, cache, cache->name);
	}

	return ret;
}

/** Free an object to a slab cache.
 * @param cache		Cache to free to.
 * @param obj		Object to free. */
void slab_cache_free(slab_cache_t *cache, void *obj) {
	assert(cache);

	if(!(cache->flags & SLAB_CACHE_NOMAG)) {
		if(likely(slab_cpu_obj_free(cache, obj))) {
#if CONFIG_SLAB_STATS
			atomic_dec(&cache->alloc_current);
#endif
			dprintf("slab: freed %p to cache %p(%s) (magazine)\n", obj, cache, cache->name);
			return;
		}
	}

	/* Cannot free to magazine layer, free to slab layer. */
	slab_obj_free(cache, obj);
#if CONFIG_SLAB_STATS
	atomic_dec(&cache->alloc_current);
#endif
	dprintf("slab: freed %p to cache %p(%s) (slab)\n", obj, cache, cache->name);
}

/** Initialise a slab cache.
 * @param cache		Cache to initialise.
 * @param name		Name of cache (for debugging purposes).
 * @param size		Size of each object.
 * @param align		Alignment of each object. Must be a power of two.
 * @param ctor		Constructor callback - performs one-time initialisation
 *			of an object (optional).
 * @param dtor		Destructor callback - undoes anything done by the
 *			constructor, if applicable (optional).
 * @param data		Data to pass as second parameter to callback functions.
 * @param priority	Reclaim priority (lower values will be reclaimed before
 *			higher values).
 * @param source	Arena used to allocate memory. If NULL, the kernel heap
 * 			arena will be used.
 * @param flags		Flags to modify the behaviour of the cache.
 * @param kmflag	Allocation flags.
 * @return		Whether the cache was successfully initialised. */
static bool slab_cache_init(slab_cache_t *cache, const char *name, size_t size, size_t align,
                            slab_ctor_t ctor, slab_dtor_t dtor, void *data, int priority,
                            vmem_t *source, int flags, int kmflag) {
	slab_cache_t *exist;

	assert(size);
	assert(source);
	assert(source->quantum >= SLAB_ALIGN_MIN);
	assert(align == 0 || !(align & (align - 1)));
	assert(!(flags & SLAB_CACHE_LATEMAG));

	mutex_init(&cache->depot_lock, "slab_depot_lock", 0);
	mutex_init(&cache->slab_lock, "slab_slab_lock", 0);
	list_init(&cache->magazine_full);
	list_init(&cache->magazine_empty);
	list_init(&cache->slab_partial);
	list_init(&cache->slab_full);
	list_init(&cache->header);

#if CONFIG_SLAB_STATS
	atomic_set(&cache->alloc_current, 0);
	atomic_set(&cache->alloc_total, 0);
#endif
	cache->slab_count = 0;

	memset(cache->bufctl_hash, 0, sizeof(cache->bufctl_hash));

	cache->source = source;
	cache->ctor = ctor;
	cache->dtor = dtor;
	cache->data = data;
	cache->priority = priority;

	/* Alignment must be at lest SLAB_ALIGN_MIN. */
	if(align < SLAB_ALIGN_MIN) {
		align = SLAB_ALIGN_MIN;
	}

	/* Ensure that the slab size is aligned. */
	size = ROUND_UP(size, align);

	/* If we want the magazine layer to be enabled but initialisation has
	 * not been fully performed, disable it for now. */
	if(!(flags & SLAB_CACHE_NOMAG) && !slab_inited) {
		flags |= (SLAB_CACHE_NOMAG | SLAB_CACHE_LATEMAG);
	}

	/* If the cache contains large objects or is a quantum cache for vmem,
	 * do not store the metadata within allocated buffers. */
	if(flags & SLAB_CACHE_QCACHE || size >= (source->quantum / SLAB_LARGE_FRACTION)) {
		flags |= SLAB_CACHE_NOTOUCH;
	}

	/* Create the CPU cache if required. */
	if(!(flags & SLAB_CACHE_NOMAG)) {
		if(!slab_cpu_cache_init(cache, kmflag)) {
			return false;
		}
	}

	/* Set calculated settings for the cache. */
	cache->colour_next = 0;
	cache->flags = flags;
	cache->obj_size = size;
	cache->align = align;
	cache->slab_size = slab_get_slabsize(cache);
	if(flags & SLAB_CACHE_NOTOUCH) {
		cache->obj_count = cache->slab_size / cache->obj_size;
		cache->colour_max = cache->slab_size - (cache->obj_count * cache->obj_size);
	} else {
		cache->obj_count = (cache->slab_size - sizeof(slab_t)) / cache->obj_size;
		cache->colour_max = (cache->slab_size - (cache->obj_count * cache->obj_size)) - sizeof(slab_t);
	}

	strncpy(cache->name, name, SLAB_NAME_MAX);
	cache->name[SLAB_NAME_MAX - 1] = 0;

	/* Add the cache to the global cache list, keeping it ordered by
	 * priority. */
	mutex_lock(&slab_caches_lock);
	if(list_empty(&slab_caches)) {
		list_append(&slab_caches, &cache->header);
	} else {
		LIST_FOREACH(&slab_caches, iter) {
			exist = list_entry(iter, slab_cache_t, header);

			if(exist->priority > priority) {
				list_add_before(&exist->header, &cache->header);
				break;
			} else if(exist->header.next == &slab_caches) {
				list_append(&slab_caches, &cache->header);
				break;
			}
		}
	}
	mutex_unlock(&slab_caches_lock);

	dprintf("slab: created slab cache %p(%s) (objsize: %u, slabsize: %u, align: %u)\n",
		cache, cache->name, cache->obj_size, cache->slab_size, cache->align);
	return true;
}

/** Create a slab cache.
 * @param name		Name of cache (for debugging purposes).
 * @param size		Size of each object.
 * @param align		Alignment of each object. Must be a power of two.
 * @param ctor		Constructor callback - performs one-time initialisation
 *			of an object (optional).
 * @param dtor		Destructor callback - undoes anything done by the
 *			constructor, if applicable (optional).
 * @param data		Data to pass as second parameter to callback functions.
 * @param source	Arena used to allocate memory. If NULL, the kernel heap
 *			arena will be used.
 * @param flags		Flags to modify the behaviour of the cache.
 * @param kmflag	Allocation flags.
 * @return		Pointer to cache on success, negative error code on
 *			failure. */
slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align,
                                slab_ctor_t ctor, slab_dtor_t dtor, void *data,
                                vmem_t *source, int flags, int kmflag) {
	slab_cache_t *cache;
	int priority;

	/* Use the kernel heap if no specific source is provided. */
	if(source == NULL) {
		source = &kheap_arena;
	}

	/* If the cache is a quantum cache, reclaim it after all other caches.
	 * This means that heap space reclaimed from normal caches can then be
	 * reclaimed from the quantum caches. */
	if(flags & SLAB_CACHE_QCACHE) {
		priority = SLAB_QCACHE_PRIORITY;
	} else {
		priority = SLAB_DEFAULT_PRIORITY;
	}

	cache = slab_cache_alloc(&slab_cache_cache, kmflag);
	if(cache == NULL) {
		return cache;
	}

	if(!slab_cache_init(cache, name, size, align, ctor, dtor, data, priority,
	                    source, flags, kmflag)) {
		slab_cache_free(&slab_cache_cache, cache);
		return NULL;
	}

	return cache;
}

/** Destroy a slab cache.
 * @param cache		Cache to destroy. */
void slab_cache_destroy(slab_cache_t *cache) {
	assert(cache);

	/* Destroy all magazines. */
	slab_cache_reclaim(cache, true);

	mutex_lock(&cache->slab_lock);
	if(!list_empty(&cache->slab_partial) || !list_empty(&cache->slab_full)) {
		fatal("Cache %s still has allocations during destruction", cache->name);
	}
	mutex_unlock(&cache->slab_lock);

	mutex_lock(&slab_caches_lock);
	list_remove(&cache->header);
	mutex_unlock(&slab_caches_lock);

	slab_cache_free(&slab_cache_cache, cache);
}

/** Prints a list of all slab caches.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success. */
int kdbg_cmd_slab(int argc, char **argv) {
	slab_cache_t *cache;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all active slab caches and some statistics about them.\n");
		return KDBG_OK;
	}

#if CONFIG_SLAB_STATS
	kprintf(LOG_NONE, "Name                      Align  Obj Size Slab Size Flags Slab Count Current Total\n");
	kprintf(LOG_NONE, "====                      =====  ======== ========= ===== ========== ======= =====\n");
#else
	kprintf(LOG_NONE, "Name                      Align  Obj Size Slab Size Flags Slab Count\n");
	kprintf(LOG_NONE, "====                      =====  ======== ========= ===== ==========\n");
#endif

	LIST_FOREACH(&slab_caches, iter) {
		cache = list_entry(iter, slab_cache_t, header);

#if CONFIG_SLAB_STATS
		kprintf(LOG_NONE, "%-*s %-6zu %-8zu %-9zu %-5d %-10zu %-7d %d\n",
		        SLAB_NAME_MAX, cache->name, cache->align, cache->obj_size,
		        cache->slab_size, cache->flags, cache->slab_count,
		        atomic_get(&cache->alloc_current), atomic_get(&cache->alloc_total));
#else
		kprintf(LOG_NONE, "%-*s %-6zu %-8zu %-9zu %-5d %zu\n",
		        SLAB_NAME_MAX, cache->name, cache->align, cache->obj_size,
		        cache->slab_size, cache->flags, cache->slab_count);
#endif
	}

	return KDBG_OK;
}

/** Enable the magazine layer. */
void __init_text slab_late_init(void) {
	slab_cache_t *cache;

	mutex_lock(&slab_caches_lock);

	slab_inited = true;
	LIST_FOREACH(&slab_caches, iter) {
		cache = list_entry(iter, slab_cache_t, header);

		if(cache->flags & SLAB_CACHE_LATEMAG) {
			assert(cache->flags & SLAB_CACHE_NOMAG);
			slab_cpu_cache_init(cache, MM_FATAL);
			cache->flags &= ~(SLAB_CACHE_LATEMAG | SLAB_CACHE_NOMAG);
		}
	}

	mutex_unlock(&slab_caches_lock);
}

/** Initialise the slab allocator. */
void __init_text slab_init(void) {
	/* Initialise the metadata arena. */
	vmem_early_create(&slab_metadata_arena, "slab_metadata_arena", 0, 0, PAGE_SIZE,
	                  kheap_anon_afunc, kheap_anon_ffunc, &kheap_raw_arena, 0,
	                  0, MM_FATAL);

	/* Initialise statically allocated internal caches. */
	slab_cache_init(&slab_cache_cache, "slab_cache_cache", sizeof(slab_cache_t), 0, NULL,
	                NULL, NULL, SLAB_METADATA_PRIORITY, &slab_metadata_arena, 0, MM_FATAL);
	slab_cache_init(&slab_bufctl_cache, "slab_bufctl_cache", sizeof(slab_bufctl_t), 0, NULL,
	                NULL, NULL, SLAB_METADATA_PRIORITY, &slab_metadata_arena, 0, MM_FATAL);
	slab_cache_init(&slab_slab_cache, "slab_slab_cache", sizeof(slab_t), 0, NULL, NULL, NULL,
	                SLAB_METADATA_PRIORITY, &slab_metadata_arena, 0, MM_FATAL);
	slab_cache_init(&slab_mag_cache, "slab_mag_cache", sizeof(slab_magazine_t), 0, NULL,
	                NULL, NULL, SLAB_MAG_PRIORITY, &slab_metadata_arena, SLAB_CACHE_NOMAG,
	                MM_FATAL);
}
