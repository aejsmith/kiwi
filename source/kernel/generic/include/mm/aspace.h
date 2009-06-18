/* Kiwi address space management
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
 * @brief		Address space management.
 */

#ifndef __MM_ASPACE_H
#define __MM_ASPACE_H

#include <arch/memmap.h>

#include <cpu/cpu.h>

#include <mm/page.h>

#include <sync/mutex.h>

#include <types/avltree.h>
#include <types/refcount.h>

struct aspace_source;

/** Address space region backend structure. */
typedef struct aspace_backend {
	/** Check whether a source can be mapped using the given parameters.
	 * @param source	Source being mapped.
	 * @param offset	Offset of the mapping in the source.
	 * @param size		Size of the mapping.
	 * @param flags		Flags the mapping is being created with.
	 * @return		0 if mapping allowed, negative error code
	 *			explaining why it is not allowed if not. */
	int (*map)(struct aspace_source *source, offset_t offset, size_t size, int flags);

	/** Get a page from the given source.
	 * @param source	Source to get page from.
	 * @param offset	Offset to get page from (the offset into the
	 *			region the fault occurred at, plus the offset
	 *			of the region into its source).
	 * @param addrp		Where to store address of page obtained.
	 * @return		0 on success, negative error code on failure. */
	int (*get)(struct aspace_source *source, offset_t offset, phys_ptr_t *addrp);

	/** Release a page.
	 * @param source	Source page was from.
	 * @param offset	Offset of page no longer being used. */
	void (*release)(struct aspace_source *source, offset_t offset);

	/** Clean up any data associated with a source. Called when source
	 * reference count reaches 0.
	 * @param source	Source being destroyed. */
	void (*destroy)(struct aspace_source *source);
} aspace_backend_t;

/** Address space page source structure. */
typedef struct aspace_source {
	aspace_backend_t *backend;	/**< Backend for the region. */
	void *data;			/**< Data for the backend. */
	refcount_t count;		/**< Count of regions using the source. */
	char *name;			/**< Name of the source. */
} aspace_source_t;

/** Address space region structure. */
typedef struct aspace_region {
	ptr_t start;			/**< Base address of the region. */
	ptr_t end;			/**< Size of the region. */
	int flags;			/**< Flags for the region. */

	aspace_source_t *source;	/**< Source of pages. */
	offset_t offset;		/**< Offset into the page source. */

	avltree_node_t *node;		/**< AVL tree node for the region. */
} aspace_region_t;

/** Address space structure. */
typedef struct aspace {
	mutex_t lock;			/**< Lock to protect address space. */
	refcount_t count;		/**< Reference count of CPUs using address space. */

	page_map_t pmap;		/**< Underlying page map for address space. */
	avltree_t regions;		/**< Tree of memory regions. */

	aspace_region_t *find_cache;	/**< Cached pointer to last region searched for. */
} aspace_t;

/** Macro that expands to a pointer to the current address space. */
#define curr_aspace		(curr_cpu->aspace)

/** Address space region flags. */
#define AS_REGION_READ		(1<<0)	/**< Mapping should be readable. */
#define AS_REGION_WRITE		(1<<1)	/**< Mapping should be writable. */
#define AS_REGION_EXEC		(1<<2)	/**< Mapping should be executable. */
#define AS_REGION_PRIVATE	(1<<3)	/**< Mapping should be private and never shared. */
#define AS_REGION_RESERVED	(1<<4)	/**< Region is reserved and should never be allocated. */

/** Page fault reason codes. */
#define PF_REASON_NPRES		1	/**< Fault caused by a not present page. */
#define PF_REASON_PROT		2	/**< Fault caused by a protection violation. */

/** Page fault access codes. */
#define PF_ACCESS_READ		1	/**< Fault caused by a read. */
#define PF_ACCESS_WRITE		2	/**< Fault caused by a write. */
#define PF_ACCESS_EXEC		3	/**< Fault when trying to execute. */

/** Page fault status codes. */
#define PF_STATUS_OK		1	/**< Fault was handled and execution can resume. */
#define PF_STATUS_FAULT		2	/**< Fault could not be handled. */

/** Check if a range fits in an address space. */
#if ASPACE_BASE == 0
# define aspace_region_fits(start, size) (((start) + (size)) <= ASPACE_SIZE)
#else
# define aspace_region_fits(start, size) ((start) >= ASPACE_BASE && ((start) + (size)) <= (ASPACE_BASE + ASPACE_SIZE))
#endif

/** Architecture functions. */
extern int aspace_arch_create(aspace_t *as);

/** Helper functions. */
extern aspace_source_t *aspace_source_alloc(const char *name);

/** Anonymous backend functions. */
extern int aspace_anon_create(aspace_source_t **sourcep);

/** Address space manipulation functions. */
extern int aspace_alloc(aspace_t *as, size_t size, int flags, aspace_source_t *source, offset_t offset, ptr_t *addrp);
extern int aspace_insert(aspace_t *as, ptr_t start, size_t size, int flags, aspace_source_t *source, offset_t offset);
extern int aspace_free(aspace_t *as, ptr_t start, size_t size);

/** Core functions. */
extern int aspace_pagefault(ptr_t addr, int reason, int access);
extern void aspace_switch(aspace_t *as);
extern aspace_t *aspace_create(void);
extern void aspace_destroy(aspace_t *as);

extern void aspace_init(void);

extern int kdbg_cmd_aspace(int argc, char **argv);

#endif /* __MM_ASPACE_H */
