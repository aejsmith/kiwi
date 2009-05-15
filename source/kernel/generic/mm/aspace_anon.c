/* Kiwi anonymous address space backend
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
 * @brief		Anonymous address space backend.
 */

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/pmm.h>

#include <assert.h>
#include <errors.h>

/** Get a page from an anonymous source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @param addrp		Where to store address of page.
 * @return		True on success, False on failure. */
static bool aspace_anon_get(aspace_source_t *source, offset_t offset, phys_ptr_t *addrp) {
	phys_ptr_t page = pmm_alloc(1, MM_SLEEP);

	*addrp = page;
	return true; 
}

/** Release a page in an anonymous source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @return		Pointer to page allocated, or NULL on failure. */
static void aspace_anon_release(aspace_source_t *source, offset_t offset) {
	/* TODO */
	return;
}

/** Destroy data in an anonymous source.
 * @param source	Source to destroy. */
static void aspace_anon_destroy(aspace_source_t *source) {
	return;
}

/** Anonymous address space backend structure. */
static aspace_backend_t aspace_anon_backend = {
	.get = aspace_anon_get,
	.release = aspace_anon_release,
	.destroy = aspace_anon_destroy,
};

/** Create a new anonymous page source.
 *
 * Creates a new anonymous page source to use to back an address space region.
 * The structure returned can be passed to aspace_insert() and aspace_alloc().
 *
 * @param sourcep	Where to store pointer to source structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_anon_create(aspace_source_t **sourcep) {
	aspace_source_t *source = aspace_source_alloc();

	assert(sourcep);

	source->backend = &aspace_anon_backend;
	source->data = NULL;
	source->offset = 0;

	*sourcep = source;
	return 0;
}
