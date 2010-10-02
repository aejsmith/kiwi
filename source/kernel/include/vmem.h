/*
 * Copyright (C) 2008-2009 Alex Smith
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

/** Vmem limitations/settings. */
#define VMEM_NAME_MAX		25		/**< Maximum name length of a Vmem arena. */
#define VMEM_HASH_INITIAL	16		/**< Initial size of allocation hash table. */
#define VMEM_QCACHE_MAX		16		/**< Maximum number of quantum caches. */
#define VMEM_REFILL_THRESHOLD	16		/**< Minimum number of boundary tags before refilling. */
#define VMEM_BOOT_TAG_COUNT	64		/**< Number of boundary tags to statically allocate. */
#define VMEM_RETRY_INTERVAL	1000000		/**< Interval between retries when sleeping for space (in µs). */
#define VMEM_RETRY_MAX		30		/**< Maximum number of VMEM_RETRY_INTERVAL-long iterations. */

/** Number of free lists to use. */
#define VMEM_FREELISTS		BITS(vmem_resource_t)

/** Slab size to use for a vmem quantum cache. */
#define VMEM_QCACHE_SSIZE(m)	MAX(1 << highbit(3 * (m)), 64)

/** Type of vmem-allocated resources. */
typedef uint64_t vmem_resource_t;

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

/** Vmem allocation function type. */
typedef vmem_resource_t (*vmem_afunc_t)(struct vmem *, vmem_resource_t, int);

/** Vmem free function type. */
typedef void (*vmem_ffunc_t)(struct vmem *, vmem_resource_t, vmem_resource_t);

/** Vmem arena structure. */
typedef struct vmem {
	mutex_t lock;				/**< Lock to protect arena. */
	size_t quantum;				/**< Quantum (size of each allocation). */
	size_t qcache_max;			/**< Maximum size to cache. */
	size_t qshift;				/**< log2(quantum). */
	uint32_t type;				/**< Resource type. */

	/** Quantum cache array. */
	struct slab_cache *qcache[VMEM_QCACHE_MAX];

	/** Boundary tag lists. */
	list_t free[VMEM_FREELISTS];		/**< Power-of-2 free segment list. */
	vmem_resource_t freemap;		/**< Bitmap of free lists - set bit indicates a list has segments. */
	list_t init_hash[VMEM_HASH_INITIAL];	/**< Initial allocation hash table. */
	list_t *alloc;				/**< Allocation hash table. */
	size_t htbl_size;			/**< Current size of allocation hash table. */
	list_t btags;				/**< List of boundary tags. */
	condvar_t space_cvar;			/**< Condition variable to wait for space on. */

	/** Source information. */
	vmem_afunc_t afunc;			/**< Allocation function. */
	vmem_ffunc_t ffunc;			/**< Free function. */
	struct vmem *source;			/**< Source arena. */

	/** Various statistics. */
	list_t children;			/**< List of arenas using this arena as a source. */
	vmem_resource_t total_size;		/**< Total size of all spans. */
	vmem_resource_t used_size;		/**< Total size of all in-use segments. */
	vmem_resource_t imported_size;		/**< Total size of all imported spans. */
	size_t alloc_count;			/**< Total number of allocations that have taken place. */

	/** Debugging information. */
	list_t header;				/**< Link to arena/children list. */
	char name[VMEM_NAME_MAX];		/**< Name of the arena. */
} vmem_t;

/** Flags for Vmem functions. */
#define VM_BESTFIT		(1<<10)		/**< Use the smallest free segment suitable for the allocation. */

extern vmem_resource_t vmem_xalloc(vmem_t *vmem, vmem_resource_t size,
                                   vmem_resource_t align, vmem_resource_t phase,
                                   vmem_resource_t nocross, vmem_resource_t minaddr,
                                   vmem_resource_t maxaddr, int vmflag);
extern void vmem_xfree(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size);

extern vmem_resource_t vmem_alloc(vmem_t *vmem, vmem_resource_t size, int vmflag);
extern void vmem_free(vmem_t *vmem, vmem_resource_t addr, vmem_resource_t size);

extern bool vmem_add(vmem_t *vmem, vmem_resource_t base, vmem_resource_t size, int vmflag);

extern bool vmem_early_create(vmem_t *vmem, const char *name, vmem_resource_t base, vmem_resource_t size,
                              size_t quantum, vmem_afunc_t afunc, vmem_ffunc_t ffunc, vmem_t *source,
                              size_t qcache_max, uint32_t type, int vmflag);
extern vmem_t *vmem_create(const char *name, vmem_resource_t base, vmem_resource_t size, size_t quantum,
                           vmem_afunc_t afunc, vmem_ffunc_t ffunc, vmem_t *source,
                           size_t qcache_max, uint32_t type, int vmflag);

extern void vmem_early_init(void);
extern void vmem_init(void);

extern int kdbg_cmd_vmem(int argc, char **argv);

#endif /* __VMEM_H */
