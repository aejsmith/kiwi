/*
 * Copyright (C) 2008-2009 Alex Smith
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
 */

#ifndef __VMEM_H
#define __VMEM_H

#include <lib/list.h>
#include <lib/utility.h>

#include <mm/flags.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <lrm.h>

struct slab_cache;
struct vmem;

/** Sizes of entries in the vmem structure. */
#define VMEM_NAME_MAX		25		/**< Maximum name length of a Vmem arena. */
#define VMEM_HASH_INITIAL	16		/**< Initial size of allocation hash table. */
#define VMEM_QCACHE_MAX		16		/**< Maximum number of quantum caches. */

/** Slab size to use for a quantum cache. */
#define VMEM_QCACHE_SSIZE(m)	MAX(1 << highbit(3 * (m)), 64)

/** Number of free lists to use. */
#define VMEM_FREELISTS		((int)BITS(vmem_resource_t))

/** Type of vmem-allocated resources. */
typedef uint64_t vmem_resource_t;

/** Source import callback function type.
 * @param base		Base of imported span.
 * @param size		Size of imported span.
 * @param vmflag	Allocation flags.
 * @return		Status code describing result of the operation. */
typedef status_t (*vmem_import_t)(vmem_resource_t base, vmem_resource_t size, int vmflag);

/** Source release callback function type.
 * @param base		Base of span being released.
 * @param size		Size of span being released. */
typedef void (*vmem_release_t)(vmem_resource_t base, vmem_resource_t size);

/** Vmem arena structure. */
typedef struct vmem {
	mutex_t lock;				/**< Lock to protect arena. */
	size_t quantum;				/**< Quantum (size of each allocation). */
	size_t qcache_max;			/**< Maximum size to cache. */
	size_t qshift;				/**< log2(quantum). */
	uint32_t type;				/**< Resource type. */
	int flags;				/**< Arena behaviour flags. */

	/** Quantum cache array. */
	struct slab_cache *qcache[VMEM_QCACHE_MAX];

	/** Boundary tag lists. */
	list_t free[VMEM_FREELISTS];		/**< Power-of-2 free segment list. */
	vmem_resource_t free_map;		/**< Bitmap of free lists containing segments. */
	list_t *alloc_hash;			/**< Allocation hash table. */
	size_t alloc_hash_size;			/**< Current size of allocation hash table. */
	list_t initial_hash[VMEM_HASH_INITIAL];	/**< Initial allocation hash table. */
	bool rehash_requested;			/**< Whether a rehash has been requested. */
	list_t btags;				/**< List of boundary tags. */
	condvar_t space_cvar;			/**< Condition variable to wait for space on. */

	/** Source information. */
	struct vmem *source;			/**< Source arena. */
	vmem_import_t import;			/**< Source import callback. */
	vmem_release_t release;			/**< Source release callback. */

	/** Various statistics. */
	vmem_resource_t total_size;		/**< Total size of all spans. */
	vmem_resource_t used_size;		/**< Total size of all in-use segments. */
	vmem_resource_t imported_size;		/**< Total size of all imported spans. */
	vmem_resource_t used_segs;		/**< Number of currently used segments. */
	size_t alloc_count;			/**< Total number of allocations that have taken place. */

	/** Other information. */
	list_t header;				/**< Link to arena list. */
	list_t children;			/**< List of arenas using this arena as a source. */
	list_t parent_link;			/**< Link to parent arena. */
	char name[VMEM_NAME_MAX];		/**< Name of the arena. */
} vmem_t;

/** Arena behaviour flags. */
#define VMEM_REFILL		(1<<0)		/**< Arena is on the refill allocation path. */

/** Allocation behaviour flags for vmem. */
#define VM_BESTFIT		(1<<10)		/**< Use the smallest free segment suitable for the allocation. */

extern vmem_resource_t vmem_xalloc(vmem_t *vmem, vmem_resource_t size, vmem_resource_t align,
                                   vmem_resource_t nocross, vmem_resource_t minaddr,
                                   vmem_resource_t maxaddr, int vmflag);
extern void vmem_xfree(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size);

extern vmem_resource_t vmem_alloc(vmem_t *vmem, vmem_resource_t size, int vmflag);
extern void vmem_free(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size);

extern bool vmem_add(vmem_t *vmem, vmem_resource_t base, vmem_resource_t size, int vmflag);

extern bool vmem_early_create(vmem_t *vmem, const char *name, size_t quantum, uint32_t type,
                              int flags, vmem_t *source, vmem_import_t import, vmem_release_t release,
                              size_t qcache_max, vmem_resource_t base, vmem_resource_t size,
                              int vmflag);
extern vmem_t *vmem_create(const char *name, size_t quantum, uint32_t type, int flags,
                           vmem_t *source, vmem_import_t import, vmem_release_t release,
                           size_t qcache_max, vmem_resource_t base, vmem_resource_t size,
                           int vmflag);

extern int kdbg_cmd_vmem(int argc, char **argv);

extern void vmem_early_init(void);
extern void vmem_init(void);
extern void vmem_late_init(void);

#endif /* __VMEM_H */
