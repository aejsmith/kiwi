/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Slab allocator.
 */

#pragma once

#include <arch/page.h>

#include <lib/list.h>

#include <mm/mm.h>

#include <sync/mutex.h>

struct slab_percpu;
struct slab_bufctl;

/** Allocator limitations/settings. */
#define SLAB_NAME_MAX           32      /**< Maximum slab cache name length. */
#define SLAB_MAGAZINE_SIZE      16      /**< Initial magazine size (resizing currently not supported). */
#define SLAB_HASH_SIZE          64      /**< Allocation hash table size. */
#define SLAB_ALIGN_MIN          8       /**< Minimum alignment. */
#define SLAB_LARGE_FRACTION     8       /**< Minimum fraction of the source quantum for large objects. */
#define SLAB_WASTE_FRACTION     8       /**< Maximum fraction of a slab that should be wasted. */

/** Slab constructor callback function. */
typedef void (*slab_ctor_t)(void *obj, void *data);

/** Slab destructor callback function. */
typedef void (*slab_dtor_t)(void *obj, void *data);

/** Slab cache structure. */
typedef struct slab_cache {
    struct slab_percpu *cpu_caches;     /**< Per-CPU caches. */

    /** Magazine depot structures. */
    mutex_t depot_lock;                 /**< Magazine depot lock. */
    list_t magazine_full;               /**< List of full magazines. */
    list_t magazine_empty;              /**< List of empty magazines. */

    /** Statistics. */
    #if CONFIG_SLAB_STATS
        atomic_uint alloc_total;        /**< Total number of allocations that have been made. */
        atomic_uint alloc_current;      /**< Number of currently allocated objects. */
    #endif
    size_t slab_count;                  /**< Number of allocated slabs. */

    /** Slab lists/cache colouring settings. */
    mutex_t slab_lock;                  /**< Lock to protect slab lists. */
    list_t slab_partial;                /**< List of partially allocated slabs. */
    list_t slab_full;                   /**< List of fully allocated slabs. */
    size_t colour_next;                 /**< Next cache colour. */
    size_t colour_max;                  /**< Maximum cache colour. */

    /** Allocation hash table for no-touch caches. */
    struct slab_bufctl *bufctl_hash[SLAB_HASH_SIZE];

    /** Cache settings. */
    unsigned flags;                     /**< Cache behaviour flags. */
    size_t slab_size;                   /**< Size of a slab. */
    size_t obj_size;                    /**< Size of an object. */
    size_t obj_count;                   /**< Number of objects per slab. */
    size_t align;                       /**< Required alignment of each object. */

    /** Callback functions. */
    slab_ctor_t ctor;                   /**< Object constructor function. */
    slab_dtor_t dtor;                   /**< Object destructor function. */
    void *data;                         /**< Data to pass to helper functions. */
    int priority;                       /**< Reclaim priority. */

    /** Debugging information. */
    list_t link;                        /**< List to slab cache list. */
    char name[SLAB_NAME_MAX];           /**< Name of cache. */
#if CONFIG_SLAB_GUARD
    size_t orig_obj_size;               /**< Original requested object size. */
#endif
} __cacheline_aligned slab_cache_t;

/** Slab cache flags. */
enum {
    SLAB_CACHE_NO_MAG       = (1<<0),   /**< Disable the magazine layer. */
    SLAB_CACHE_LARGE        = (1<<1),   /**< Cache is a large object cache. */

    __SLAB_CACHE_LATE_MAG   = (1<<3),
    __SLAB_CACHE_NO_GUARD   = (1<<4),
};

extern void *slab_cache_alloc(slab_cache_t *cache, uint32_t mmflag);
extern void slab_cache_free(slab_cache_t *cache, void *obj);

extern slab_cache_t *slab_cache_create(
    const char *name, size_t size, size_t align, slab_ctor_t ctor,
    slab_dtor_t dtor, void *data, uint32_t flags, uint32_t mmflag);
extern void slab_cache_destroy(slab_cache_t *cache);

/** Create a slab cache for allocation of objects of a certain type.
 * @param name          Name of cache (for debugging purposes).
 * @param type          Type of object to allocate.
 * @param ctor          Constructor callback (optional).
 * @param dtor          Destructor callback (optional).
 * @param data          Data to pass as second parameter to callback functions.
 * @param flags         Flags to modify the behaviour of the cache.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to cache on success, NULL on failure. */
#define object_cache_create(name, type, ctor, dtor, data, flags, mmflag) \
    slab_cache_create(name, sizeof(type), alignof(type), ctor, dtor, data, flags, mmflag)

extern void slab_init(void);
extern void slab_late_init(void);
