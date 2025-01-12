/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Slab allocator.
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
 * TODO:
 *  - Dynamic magazine resizing.
 *  - Allocation hash table resizing.
 */

#include <lib/fnv.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "trace.h"

struct slab;

/** Slab magazine structure. */
typedef struct slab_magazine {
    /** Array of objects in the magazine. */
    void *objects[SLAB_MAGAZINE_SIZE];

    union {
        /** Number of rounds currently in the magazine (while empty). */
        size_t rounds;

        /**
         * Last used time, for magzines in the depot, to determine when to
         * reclaim.
         */
        nstime_t last_used;
    };

    list_t link;                        /**< Link to depot lists. */
} slab_magazine_t;

/** Slab per-CPU cache structure. */
typedef struct __cacheline_aligned slab_percpu {
    slab_magazine_t *loaded;            /**< Current (loaded) magazine. */
    slab_magazine_t *previous;          /**< Previous magazine. */

    /**
     * Version number. This is incremented on every completed operation on the
     * per-CPU cache. It is used to handle thread switches that occur during
     * blocking operations when we have to go to the depot. volatile to ensure
     * the compiler does not cache its value.
     */
    volatile uint32_t version;
} slab_percpu_t;

/**
 * Slab buffer control structure.
 *
 * The order of this structure is important: the pointer MUST be first, as it
 * is the only member that exists for small-object caches.
 */
typedef struct slab_bufctl {
    struct slab_bufctl *next;           /**< Address of next buffer. */

    struct slab *parent;                /**< Parent slab structure. */
    void *object;                       /**< Pointer to actual object. */
} slab_bufctl_t;

/** Slab structure. */
typedef struct slab {
    list_t link;                        /**< Link to appropriate slab list in cache. */

    void *base;                         /**< Base address of allocation. */
    size_t refcount;                    /**< Reference count. */
    slab_bufctl_t *free;                /**< List of free buffers. */
    size_t colour;                      /**< Colour of the slab. */
    slab_cache_t *parent;               /**< Cache containing the slab. */
} slab_t;

/** Reclaim priorities to use for caches. */
enum {
    SLAB_DEFAULT_PRIORITY       = 0,
    SLAB_METADATA_PRIORITY      = 1,
    SLAB_MAG_PRIORITY           = 2,
};

/** Interval for the slab reclaim thread. */
#define SLAB_RECLAIM_INTERVAL       msecs_to_nsecs(500)

/** How long a full/empty magazine has to be unused before it is reclaimed. */
#define SLAB_RECLAIM_FULL_AGE       secs_to_nsecs(5)
#define SLAB_RECLAIM_EMPTY_AGE      secs_to_nsecs(20)

/** Internally-used caches. */
static slab_cache_t slab_cache_cache;   /**< Cache for allocation of new slab caches. */
static slab_cache_t slab_mag_cache;     /**< Cache for magazine structures. */
static slab_cache_t slab_bufctl_cache;  /**< Cache for buffer control structures. */
static slab_cache_t slab_slab_cache;    /**< Cache for slab structures. */
static slab_cache_t *slab_percpu_cache; /**< Cache for per-CPU structures. */

/** List of all slab caches. */
static LIST_DEFINE(slab_caches);
static MUTEX_DEFINE(slab_caches_lock, 0);

#if CONFIG_SLAB_GUARD
static const uint8_t SLAB_GUARD[8]  = { 0xa0, 0xb1, 0xc2, 0xd3, 0xe4, 0xf5, 0x06, 0x17 };
static const size_t SLAB_GUARD_SIZE = 8;
#endif

static void slab_destroy(slab_cache_t *cache, slab_t *slab) {
    assert(!slab->refcount);

    void *addr = slab->base;

    list_remove(&slab->link);

    /* Destroy all buffer control structures and the slab structure if stored
     * externally. */
    if (cache->flags & SLAB_CACHE_LARGE) {
        while (slab->free) {
            slab_bufctl_t *bufctl = slab->free;
            slab->free = bufctl->next;

            slab_cache_free(&slab_bufctl_cache, bufctl);
        }

        slab_cache_free(&slab_slab_cache, slab);
    }

    cache->slab_count--;
    kmem_free(addr, cache->slab_size);
}

static inline slab_t *slab_create(slab_cache_t *cache, uint32_t mmflag) {
    /* Drop slab lock while creating as a reclaim may occur that wants to free
     * to this cache. */
    mutex_unlock(&cache->slab_lock);

    /* Allocate a new slab. */
    void *addr = kmem_alloc(cache->slab_size, mmflag & MM_FLAG_MASK);
    if (unlikely(!addr)) {
        mutex_lock(&cache->slab_lock);
        return NULL;
    }

    mutex_lock(&cache->slab_lock);

    /* Create the slab structure for the slab. */
    slab_t *slab;
    if (cache->flags & SLAB_CACHE_LARGE) {
        slab = slab_cache_alloc(&slab_slab_cache, mmflag & MM_FLAG_MASK);
        if (unlikely(!slab)) {
            kmem_free(addr, cache->slab_size);
            return NULL;
        }
    } else {
        slab = (slab_t *)(((ptr_t)addr + cache->slab_size) - sizeof(slab_t));
    }

    cache->slab_count++;

    list_init(&slab->link);

    slab->base     = addr;
    slab->refcount = 0;
    slab->free     = NULL;
    slab->colour   = cache->colour_next;
    slab->parent   = cache;

    /* Divide the buffer up into unconstructed, free objects. */
    slab_bufctl_t *prev = NULL;
    for (size_t i = 0; i < cache->obj_count; i++) {
        slab_bufctl_t *bufctl;
        if (cache->flags & SLAB_CACHE_LARGE) {
            bufctl = slab_cache_alloc(&slab_bufctl_cache, mmflag & MM_FLAG_MASK);
            if (unlikely(!bufctl)) {
                slab_destroy(cache, slab);
                return NULL;
            }

            bufctl->parent = slab;
            bufctl->object = (void *)((ptr_t)addr + slab->colour + (i * cache->obj_size));
        } else {
            bufctl = (slab_bufctl_t *)((ptr_t)addr + slab->colour + (i * cache->obj_size));
        }

        /* Add to the free list. */
        bufctl->next = NULL;
        if (!prev) {
            slab->free = bufctl;
        } else {
            prev->next = bufctl;
        }
        prev = bufctl;
    }

    /* Success, update the cache colour and return. Do not add to any slab
     * lists, the caller will do so. */
    cache->colour_next += cache->align;
    if (cache->colour_next > cache->colour_max)
        cache->colour_next = 0;

    return slab;
}

/** Destruct an object and free it to the slab layer. */
static inline void slab_obj_free(slab_cache_t *cache, void *obj) {
    mutex_lock(&cache->slab_lock);

    /* Find the buffer control structure. For large object caches, look it up
     * on the allocation hash table. Otherwise, we use the start of the buffer
     * as the structure. */
    slab_bufctl_t *bufctl;
    slab_t *slab;
    if (cache->flags & SLAB_CACHE_LARGE) {
        uint32_t hash = fnv32_hash_integer((ptr_t)obj) % SLAB_HASH_SIZE;
        slab_bufctl_t *prev = NULL;
        for (bufctl = cache->bufctl_hash[hash]; bufctl; bufctl = bufctl->next) {
            if (bufctl->object == obj)
                break;

            prev = bufctl;
        }

        if (unlikely(!bufctl))
            fatal("Free (%s): object %p not found in hash table", cache->name, obj);

        slab = bufctl->parent;
        assert(slab->parent == cache);

        /* Take the object off the hash chain. */
        if (prev) {
            prev->next = bufctl->next;
        } else {
            cache->bufctl_hash[hash] = bufctl->next;
        }
    } else {
        bufctl = (slab_bufctl_t *)obj;

        /* Find the slab corresponding to the object. The structure will be at
         * the end of the slab. */
        slab = (slab_t *)(round_down((ptr_t)obj, cache->slab_size) + (cache->slab_size - sizeof(slab_t)));
        if (unlikely(slab->parent != cache))
            fatal("Free (%s): slab structure for %p invalid (%p)", cache->name, obj, slab->parent);
    }

    /* Call the object destructor. */
    if (cache->dtor)
        cache->dtor(obj, cache->data);

    assert(slab->refcount);

    /* Return the object to the slab's free list. */
    bufctl->next = slab->free;
    slab->free = bufctl;

    if (--slab->refcount == 0) {
        /* Slab empty, destroy it. */
        slab_destroy(cache, slab);
    } else if ((slab->refcount + 1) == cache->obj_count) {
        /* Take from the full list and move to the partial list. */
        list_append(&cache->slab_partial, &slab->link);
    }

    mutex_unlock(&cache->slab_lock);
}

/** Allocate an object from the slab layer and construct it. */
static inline void *slab_obj_alloc(slab_cache_t *cache, uint32_t mmflag) {
    mutex_lock(&cache->slab_lock);

    /* If there is a slab in the partial list, take it. */
    slab_t *slab;
    if (!list_empty(&cache->slab_partial)) {
        slab = list_first(&cache->slab_partial, slab_t, link);
    } else {
        /* No slabs with free objects available - allocate a new slab. */
        slab = slab_create(cache, mmflag);
        if (unlikely(!slab)) {
            mutex_unlock(&cache->slab_lock);
            return NULL;
        }
    }

    assert(slab->free);

    /* Take an object from the slab. If the metadata is stored externally, then
     * the object address is contained in the object field of the bufctl
     * structure. Otherwise, the object address is the same as the structure
     * address. */
    slab_bufctl_t *bufctl = slab->free;
    slab->free = bufctl->next;
    slab->refcount++;

    void *obj = (cache->flags & SLAB_CACHE_LARGE)
        ? bufctl->object
        : (void *)bufctl;

    /* Place the allocation on the allocation hash table if required. */
    if (cache->flags & SLAB_CACHE_LARGE) {
        uint32_t hash = fnv32_hash_integer((ptr_t)obj) % SLAB_HASH_SIZE;
        bufctl->next = cache->bufctl_hash[hash];
        cache->bufctl_hash[hash] = bufctl;
    }

    /* Check if a list move is required. */
    if (slab->refcount == cache->obj_count) {
        list_append(&cache->slab_full, &slab->link);
    } else {
        list_append(&cache->slab_partial, &slab->link);
    }

    /* Construct the object and return it. Unlock the cache before calling the
     * constructor as it may cause a reclaim. */
    mutex_unlock(&cache->slab_lock);
    if (cache->ctor)
        cache->ctor(obj, cache->data);

    return obj;
}

/** Get a full magazine from a cache's depot. */
static inline slab_magazine_t *slab_magazine_get_full(slab_cache_t *cache) {
    mutex_lock(&cache->depot_lock);

    slab_magazine_t *mag = NULL;
    if (!list_empty(&cache->magazine_full)) {
        mag = list_first(&cache->magazine_full, slab_magazine_t, link);
        list_remove(&mag->link);

        mag->rounds = SLAB_MAGAZINE_SIZE;
    }

    mutex_unlock(&cache->depot_lock);
    return mag;
}

/** Return a full magazine to the depot. */
static inline void slab_magazine_put_full(slab_cache_t *cache, slab_magazine_t *mag) {
    assert(mag->rounds == SLAB_MAGAZINE_SIZE);

    mag->last_used = system_time();

    mutex_lock(&cache->depot_lock);
    list_prepend(&cache->magazine_full, &mag->link);
    mutex_unlock(&cache->depot_lock);
}

/** Get an empty magazine from a cache's depot. */
static inline slab_magazine_t *slab_magazine_get_empty(slab_cache_t *cache) {
    mutex_lock(&cache->depot_lock);

    slab_magazine_t *mag;
    if (!list_empty(&cache->magazine_empty)) {
        mag = list_first(&cache->magazine_empty, slab_magazine_t, link);
        list_remove(&mag->link);

        mag->rounds = 0;
    } else {
        /* None available, try to allocate a new structure. We do not wait for
         * memory to be available here as if a new magazine cannot be allocated
         * on the first try it means that the system is low on memory. In this
         * case, the object should be freed back to the source. TODO: If low on
         * memory, should not attempt to allocate at all. */
        mag = slab_cache_alloc(&slab_mag_cache, MM_ATOMIC);
        if (mag) {
            list_init(&mag->link);
            mag->rounds = 0;
        }
    }

    mutex_unlock(&cache->depot_lock);
    return mag;
}

/** Return an empty magazine to the depot. */
static inline void slab_magazine_put_empty(slab_cache_t *cache, slab_magazine_t *mag) {
    assert(!mag->rounds);

    mag->last_used = system_time();

    mutex_lock(&cache->depot_lock);
    list_prepend(&cache->magazine_empty, &mag->link);
    mutex_unlock(&cache->depot_lock);
}

/** Destroy a magazine. */
static inline void slab_magazine_destroy(slab_cache_t *cache, slab_magazine_t *mag, size_t rounds) {
    size_t i;

    /* Free all rounds within the magazine, if any. */
    for (i = 0; i < rounds; i++)
        slab_obj_free(cache, mag->objects[i]);

    list_remove(&mag->link);
    slab_cache_free(&slab_mag_cache, mag);
}

/** Allocate an object from the magazine layer. */
static inline void *slab_cpu_obj_alloc(slab_cache_t *cache) {
    /*
     * We do not have locking on the per-CPU cache as it will not be used by
     * any other CPUs. However there is the possibility that a thread switch
     * will occur on this CPU. Furthermore, normally, a thread switch could
     * result in this thread migrating to a different CPU when it resumes.
     *
     * A thread switch staying on the CPU means that our cache state might have
     * changed when we resume, due to another thread that ran using the cache.
     * A CPU switch would mean that the cache we're using is no longer correct
     * for the CPU we're running on.
     *
     * To prevent these issues, we firstly disable preemption around the
     * operation. This prevents CPU migration and preemption by timer.
     *
     * That still leaves the possibility that going to the depot will result in
     * a thread switch, as those operations take a mutex and could block.
     *
     * So, each per-CPU cache has a version number, that is incremented upon
     * completion of an operation on it. We check whether this number has
     * changed after we have called a depot operation. If it has, it means
     * another thread ran and modified this cache. In that case, we drop what
     * we were doing and go back to the start.
     */
    preempt_disable();

    slab_percpu_t *cc = &cache->cpu_caches[curr_cpu->id];

    void *ret = NULL;
    while (true) {
        uint32_t version = cc->version;

        /* Check if we have a magazine to allocate from. */
        if (likely(cc->loaded)) {
            if (cc->loaded->rounds) {
                /* Loaded has rounds, allocate from it. */
                ret = cc->loaded->objects[--cc->loaded->rounds];
                cc->version++;
                break;
            } else if (cc->previous && cc->previous->rounds) {
                /* Previous has rounds, exchange them and allocate. */
                swap(cc->loaded, cc->previous);
                ret = cc->loaded->objects[--cc->loaded->rounds];
                cc->version++;
                break;
            }
        }

        /* Try to get a full magazine from the depot. */
        slab_magazine_t *mag = slab_magazine_get_full(cache);

        /* Validate that we have not changed CPU. */
        assert(cc == &cache->cpu_caches[arch_curr_cpu_volatile()->id]);

        /* Failed to allocate so just give up. */
        if (unlikely(!mag))
            break;

        /* If version has changed, another thread ran, return the magazine and
         * try again. */
        if (version != cc->version) {
            slab_magazine_put_full(cache, mag);
            continue;
        }

        slab_magazine_t *previous = cc->previous;

        assert(!previous || !previous->rounds);

        /* Load the magazine. */
        cc->previous = cc->loaded;
        cc->loaded   = mag;

        ret = cc->loaded->objects[--cc->loaded->rounds];
        cc->version++;

        /* Return previous to the depot. This must be done after allocating,
         * because it can block again. */
        if (likely(previous))
            slab_magazine_put_empty(cache, previous);

        break;
    }

    preempt_enable();
    return ret;
}

/** Free an object to the magazine layer. */
static inline bool slab_cpu_obj_free(slab_cache_t *cache, void *obj) {
    /* See slab_cpu_obj_alloc(). */
    preempt_disable();

    slab_percpu_t *cc = &cache->cpu_caches[curr_cpu->id];

    bool ret = true;
    while (true) {
        uint32_t version = cc->version;

        if (likely(cc->loaded)) {
            if (cc->loaded->rounds < SLAB_MAGAZINE_SIZE) {
                /* Loaded magazine has space, insert the object there. */
                cc->loaded->objects[cc->loaded->rounds++] = obj;
                cc->version++;
                break;
            } else if (cc->previous && cc->previous->rounds < SLAB_MAGAZINE_SIZE) {
                /* Previous has space, exchange them and insert. */
                swap(cc->loaded, cc->previous);
                cc->loaded->objects[cc->loaded->rounds++] = obj;
                cc->version++;
                break;
            }
        }

        /* Get a new empty magazine. */
        slab_magazine_t *mag = slab_magazine_get_empty(cache);

        /* Validate that we have not changed CPU. */
        assert(cc == &cache->cpu_caches[arch_curr_cpu_volatile()->id]);

        if (unlikely(!mag)) {
            ret = false;
            break;
        }

        /* If version has changed, another thread ran, return the magazine and
         * try again. */
        if (version != cc->version) {
            slab_magazine_put_empty(cache, mag);
            continue;
        }

        slab_magazine_t *previous = cc->previous;

        assert(!previous || previous->rounds == SLAB_MAGAZINE_SIZE);

        /* Load the magazine. */
        cc->previous = cc->loaded;
        cc->loaded   = mag;

        cc->loaded->objects[cc->loaded->rounds++] = obj;
        cc->version++;

        /* Return previous. */
        if (likely(previous))
            slab_magazine_put_full(cache, previous);

        break;
    }

    preempt_enable();
    return ret;
}

#if CONFIG_SLAB_TRACING

/** Function names to skip over in trace_return_address(). */
static const char *trace_skip_names[] = {
    "kmalloc", "krealloc", "kcalloc", "kfree", "kstrdup", "kstrndup",
    "kmemdup", "kbasename", "kdirname", "strdup_from_user", "strndup_from_user",
    "arrcpy_from_user",
};

/** Get the address for allocation tracing output. */
static __always_inline void *trace_return_address(void) {
    return mm_trace_return_address(trace_skip_names, array_size(trace_skip_names));
}

#endif /* CONFIG_SLAB_TRACING */

/** Allocates a constructed object from a slab cache.
 * @param cache         Cache to allocate from.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to allocated object or NULL if unable to
 *                      allocate.  */
void *slab_cache_alloc(slab_cache_t *cache, uint32_t mmflag) {
    assert(!in_interrupt());
    assert(cache);

    void *ret = NULL;

    /* Try the magazine layer first. */
    if (!(cache->flags & SLAB_CACHE_NO_MAG))
        ret = slab_cpu_obj_alloc(cache);

    /* Fall back on the slab layer. */
    if (!ret)
        ret = slab_obj_alloc(cache, mmflag);

    if (likely(ret)) {
        #if CONFIG_SLAB_GUARD
            if (!(cache->flags & __SLAB_CACHE_NO_GUARD)) {
                uint8_t *guard = (uint8_t *)ret + cache->orig_obj_size;
                memcpy(guard, SLAB_GUARD, SLAB_GUARD_SIZE);
            }
        #endif

        #if CONFIG_SLAB_STATS
            atomic_fetch_add(&cache->alloc_total, 1);
            atomic_fetch_add(&cache->alloc_current, 1);
        #endif

        #if CONFIG_SLAB_TRACING
            kprintf(LOG_DEBUG, "alloc: %p %s %pB\n", ret, cache->name, trace_return_address());
        #endif
    }

    return ret;
}

/** Frees an object to a slab cache.
 * @param cache         Cache to free to.
 * @param obj           Object to free. */
void slab_cache_free(slab_cache_t *cache, void *obj) {
    assert(!in_interrupt());
    assert(cache);

    #if CONFIG_SLAB_GUARD
        if (!(cache->flags & __SLAB_CACHE_NO_GUARD)) {
            uint8_t *guard = (uint8_t *)obj + cache->orig_obj_size;

            if (memcmp(guard, SLAB_GUARD, SLAB_GUARD_SIZE) != 0)
                fatal("Guard failure freeing object %p from %p (%s)", obj, cache, cache->name);

            /* Clear it to detect double frees. */
            memset(guard, 0, SLAB_GUARD_SIZE);
        }
    #endif

    bool freed = false;

    if (!(cache->flags & SLAB_CACHE_NO_MAG))
        freed = slab_cpu_obj_free(cache, obj);

    if (!freed)
        slab_obj_free(cache, obj);

    #if CONFIG_SLAB_STATS
        atomic_fetch_sub(&cache->alloc_current, 1);
    #endif

    #if CONFIG_SLAB_TRACING
        kprintf(LOG_DEBUG, "free: %p %s %pB\n", obj, cache->name, trace_return_address());
    #endif
}

/** Create the per-CPU data for a slab cache. */
static status_t slab_percpu_init(slab_cache_t *cache, uint32_t mmflag) {
    assert(cpu_count != 0);
    assert(slab_percpu_cache);

    cache->cpu_caches = slab_cache_alloc(slab_percpu_cache, mmflag);
    if (!cache->cpu_caches)
        return STATUS_NO_MEMORY;

    memset(cache->cpu_caches, 0, sizeof(slab_percpu_t) * (highest_cpu_id + 1));
    return STATUS_SUCCESS;
}

/** Initialize a slab cache.
 * @param cache         Cache to initialize.
 * @param name          Name of cache (for debugging purposes).
 * @param size          Size of each object.
 * @param align         Alignment of each object. Must be a power of two.
 * @param ctor          Constructor callback - performs one-time initialization
 *                      of an object (optional).
 * @param dtor          Destructor callback - undoes anything done by the
 *                      constructor, if applicable (optional).
 * @param data          Data to pass as second parameter to callback functions.
 * @param priority      Reclaim priority (lower values will be reclaimed before
 *                      higher values).
 * @param flags         Flags to modify the behaviour of the cache.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
static status_t slab_cache_init(
    slab_cache_t *cache, const char *name, size_t size, size_t align,
    slab_ctor_t ctor, slab_dtor_t dtor, void *data, int priority,
    uint32_t flags, uint32_t mmflag)
{
    assert(size);
    assert(!align || is_pow2(align));
    assert(!(flags & __SLAB_CACHE_LATE_MAG));

    mutex_init(&cache->depot_lock, "slab_depot_lock", 0);
    mutex_init(&cache->slab_lock, "slab_slab_lock", 0);
    list_init(&cache->magazine_full);
    list_init(&cache->magazine_empty);
    list_init(&cache->slab_partial);
    list_init(&cache->slab_full);
    list_init(&cache->link);

    cache->slab_count = 0;

    #if CONFIG_SLAB_STATS
        atomic_store(&cache->alloc_current, 0);
        atomic_store(&cache->alloc_total, 0);
    #endif

    memset(cache->bufctl_hash, 0, sizeof(cache->bufctl_hash));

    strncpy(cache->name, name, SLAB_NAME_MAX);
    cache->name[SLAB_NAME_MAX - 1] = 0;

    cache->flags       = flags;
    cache->ctor        = ctor;
    cache->dtor        = dtor;
    cache->data        = data;
    cache->priority    = priority;
    cache->colour_next = 0;

    /* Alignment must be at lest SLAB_ALIGN_MIN. */
    cache->align = max(SLAB_ALIGN_MIN, align);

    #if CONFIG_SLAB_GUARD
        cache->orig_obj_size = size;
        if (!(flags & __SLAB_CACHE_NO_GUARD))
            size += SLAB_GUARD_SIZE;
    #endif

    /* Make sure the object size is aligned. */
    size = round_up(size, cache->align);
    cache->obj_size = size;

    /* If the cache contains large objects, set the large flag which causes us
     * to not store metadata within allocated space. */
    if (size >= (PAGE_SIZE / SLAB_LARGE_FRACTION)) {
        cache->flags |= SLAB_CACHE_LARGE;

        /* Compute the appropriate slab size. */
        cache->slab_size = round_up(size, PAGE_SIZE);
        while ((cache->slab_size % size) > (cache->slab_size / SLAB_WASTE_FRACTION))
            cache->slab_size += PAGE_SIZE;

        cache->obj_count  = cache->slab_size / size;
        cache->colour_max = cache->slab_size - (cache->obj_count * size);
    } else {
        cache->slab_size  = PAGE_SIZE;
        cache->obj_count  = (cache->slab_size - sizeof(slab_t)) / size;
        cache->colour_max = (cache->slab_size - (cache->obj_count * size)) - sizeof(slab_t);
    }

    /* If we want the magazine layer to be enabled but the CPU count is not
     * known, disable it until it is known. */
    if (!(cache->flags & SLAB_CACHE_NO_MAG) && !slab_percpu_cache)
        cache->flags |= (SLAB_CACHE_NO_MAG | __SLAB_CACHE_LATE_MAG);

    /* Initialize the CPU caches if required. */
    if (!(cache->flags & SLAB_CACHE_NO_MAG)) {
        status_t ret = slab_percpu_init(cache, mmflag);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    mutex_lock(&slab_caches_lock);

    /* Add the cache to the global cache list, keeping it ordered by priority. */
    if (list_empty(&slab_caches)) {
        list_append(&slab_caches, &cache->link);
    } else {
        list_foreach(&slab_caches, iter) {
            slab_cache_t *exist = list_entry(iter, slab_cache_t, link);

            if (exist->priority > priority) {
                list_add_before(&exist->link, &cache->link);
                break;
            } else if (exist->link.next == &slab_caches) {
                list_append(&slab_caches, &cache->link);
                break;
            }
        }
    }

    kprintf(
        LOG_DEBUG, "slab: created cache %s (obj_size: %zu, slab_size: %zu, align: %zu)\n",
        cache->name, cache->obj_size, cache->slab_size, cache->align);

    mutex_unlock(&slab_caches_lock);
    return STATUS_SUCCESS;
}

/** Creates a slab cache.
 * @param name          Name of cache (for debugging purposes).
 * @param size          Size of each object.
 * @param align         Alignment of each object. Must be a power of two.
 * @param ctor          Constructor callback - performs one-time initialization
 *                      of an object (optional).
 * @param dtor          Destructor callback - undoes anything done by the
 *                      constructor, if applicable (optional).
 * @param data          Data to pass as second parameter to callback functions.
 * @param flags         Flags to modify the behaviour of the cache.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to cache on success, NULL on failure. */
slab_cache_t *slab_cache_create(
    const char *name, size_t size, size_t align, slab_ctor_t ctor,
    slab_dtor_t dtor, void *data, unsigned flags, uint32_t mmflag)
{
    slab_cache_t *cache = slab_cache_alloc(&slab_cache_cache, mmflag);
    if (!cache)
        return NULL;

    status_t ret = slab_cache_init(
        cache, name, size, align, ctor, dtor, data, SLAB_DEFAULT_PRIORITY,
        flags, mmflag);
    if (ret != STATUS_SUCCESS) {
        slab_cache_free(&slab_cache_cache, cache);
        return NULL;
    }

    return cache;
}

/** Destroys a slab cache.
 * @param cache         Cache to destroy. */
void slab_cache_destroy(slab_cache_t *cache) {
    assert(cache);

    /* Destroy the CPU caches. */
    if (!(cache->flags & SLAB_CACHE_NO_MAG)) {
        mutex_lock(&cache->depot_lock);

        for (size_t i = 0; i <= highest_cpu_id; i++) {
            slab_percpu_t *cc = &cache->cpu_caches[i];

            if (cc->loaded)
                slab_magazine_destroy(cache, cc->loaded, cc->loaded->rounds);

            if (cc->previous)
                slab_magazine_destroy(cache, cc->previous, cc->previous->rounds);
        }

        /* Destroy empty magazines. */
        list_foreach_safe(&cache->magazine_empty, iter) {
            slab_magazine_t *mag = list_entry(iter, slab_magazine_t, link);
            slab_magazine_destroy(cache, mag, 0);
        }

        /* Destroy full magazines. */
        list_foreach_safe(&cache->magazine_full, iter) {
            slab_magazine_t *mag = list_entry(iter, slab_magazine_t, link);
            slab_magazine_destroy(cache, mag, SLAB_MAGAZINE_SIZE);
        }

        mutex_unlock(&cache->depot_lock);

        slab_cache_free(slab_percpu_cache, cache->cpu_caches);
    }

    mutex_lock(&cache->slab_lock);

    if (!list_empty(&cache->slab_partial) || !list_empty(&cache->slab_full))
        fatal("Cache %s still has allocations during destruction", cache->name);

    mutex_unlock(&cache->slab_lock);

    mutex_lock(&slab_caches_lock);
    list_remove(&cache->link);
    mutex_unlock(&slab_caches_lock);

    slab_cache_free(&slab_cache_cache, cache);
}

static void reclaim_mag_list(
    slab_cache_t *cache, list_t *list, nstime_t curr_time, nstime_t max_age,
    size_t rounds)
{
    list_foreach_safe(list, iter) {
        slab_magazine_t *mag = list_entry(iter, slab_magazine_t, link);

        nstime_t age = curr_time - mag->last_used;
        if (age >= max_age)
            slab_magazine_destroy(cache, mag, rounds);
    }
}

/** Thread to periodically reclaim unused magazines. */
static void slab_reclaim_thread_func(void *arg1, void *arg2) {
    while (true) {
        delay(SLAB_RECLAIM_INTERVAL);

        mutex_lock(&slab_caches_lock);

        nstime_t curr_time = system_time();

        list_foreach(&slab_caches, iter) {
            slab_cache_t *cache = list_entry(iter, slab_cache_t, link);

            mutex_lock(&cache->depot_lock);

            reclaim_mag_list(cache, &cache->magazine_full, curr_time, SLAB_RECLAIM_FULL_AGE, SLAB_MAGAZINE_SIZE);
            reclaim_mag_list(cache, &cache->magazine_empty, curr_time, SLAB_RECLAIM_EMPTY_AGE, 0);

            mutex_unlock(&cache->depot_lock);
        }

        mutex_unlock(&slab_caches_lock);
    }
}

/** Prints a list of all slab caches. */
static kdb_status_t kdb_cmd_slab(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints a list of all active slab caches and some statistics about them.\n");
        return KDB_SUCCESS;
    }

    #if CONFIG_SLAB_STATS
        kdb_printf("Name                             Align  Obj Size Slab Size Flags Slab Count Current Total\n");
        kdb_printf("====                             =====  ======== ========= ===== ========== ======= =====\n");
    #else
        kdb_printf("Name                             Align  Obj Size Slab Size Flags Slab Count\n");
        kdb_printf("====                             =====  ======== ========= ===== ==========\n");
    #endif

    list_foreach(&slab_caches, iter) {
        slab_cache_t *cache = list_entry(iter, slab_cache_t, link);

        kdb_printf(
            "%-*s %-6zu %-8zu %-9zu %-5d %-10zu",
            SLAB_NAME_MAX, cache->name, cache->align, cache->obj_size,
            cache->slab_size, cache->flags, cache->slab_count);

        #if CONFIG_SLAB_STATS
            kdb_printf(" %-7d %d", atomic_load(&cache->alloc_current), atomic_load(&cache->alloc_total));
        #endif

        kdb_printf("\n");
    }

    return KDB_SUCCESS;
}

/** Initialize the slab allocator. */
__init_text void slab_init(void) {
    /* Intialise the cache for cache structures. */
    slab_cache_init(
        &slab_cache_cache, "slab_cache_cache", sizeof(slab_cache_t),
        alignof(slab_cache_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
        __SLAB_CACHE_NO_GUARD, MM_BOOT);

    /* Initialize the magazine cache. This cannot have the magazine layer
     * enabled, for pretty obvious reasons. */
    slab_cache_init(
        &slab_mag_cache, "slab_mag_cache", sizeof(slab_magazine_t),
        alignof(slab_magazine_t), NULL, NULL, NULL, SLAB_MAG_PRIORITY,
        SLAB_CACHE_NO_MAG | __SLAB_CACHE_NO_GUARD, MM_BOOT);

    /* Create other internal caches. */
    slab_cache_init(
        &slab_bufctl_cache, "slab_bufctl_cache", sizeof(slab_bufctl_t),
        alignof(slab_bufctl_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
        __SLAB_CACHE_NO_GUARD, MM_BOOT);
    slab_cache_init(
        &slab_slab_cache, "slab_slab_cache", sizeof(slab_t),
        alignof(slab_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
        __SLAB_CACHE_NO_GUARD, MM_BOOT);

    /* Register the KDB command. */
    kdb_register_command("slab", "Display slab cache statistics.", kdb_cmd_slab);
}

/** Enable the magazine layer. */
__init_text void slab_late_init(void) {
    /* Create the cache for per-CPU structures. */
    size_t size = sizeof(slab_percpu_t) * (highest_cpu_id + 1);
    slab_percpu_cache = slab_cache_alloc(&slab_cache_cache, MM_BOOT);
    slab_cache_init(
        slab_percpu_cache, "slab_percpu_cache", size, alignof(slab_percpu_t),
        NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
        SLAB_CACHE_NO_MAG | __SLAB_CACHE_NO_GUARD, MM_BOOT);

    mutex_lock(&slab_caches_lock);

    /* Create per-CPU structures for all caches that want the magazine layer. */
    list_foreach(&slab_caches, iter) {
        slab_cache_t *cache = list_entry(iter, slab_cache_t, link);

        if (cache->flags & __SLAB_CACHE_LATE_MAG) {
            assert(cache->flags & SLAB_CACHE_NO_MAG);
            slab_percpu_init(cache, MM_BOOT);
            cache->flags &= ~(__SLAB_CACHE_LATE_MAG | SLAB_CACHE_NO_MAG);
        }
    }

    mutex_unlock(&slab_caches_lock);
}

/** Create slab worker threads. */
__init_text void slab_thread_init(void) {
    status_t ret = thread_create("slab_reclaim", NULL, 0, slab_reclaim_thread_func, NULL, NULL, NULL);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create slab reclaim thread: %" PRId32, ret);
}

INITCALL(slab_thread_init);
