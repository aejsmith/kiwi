/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Memory area functions.
 *
 * @todo		Pages for areas without a backing object should be
 *			marked as pageable.
 * @todo		Make this usable in the kernel, and allow an area
 *			covering a physical memory region to be created.
 */

#include <lib/avl_tree.h>
#include <lib/refcount.h>

#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <public/area.h>

#include <sync/mutex.h>
#include <sync/rwlock.h>

#include <assert.h>
#include <init.h>
#include <object.h>
#include <status.h>
#include <vmem.h>

/** Structure containing details of a memory area. */
typedef struct area {
	object_t obj;			/**< Object header. */

	area_id_t id;			/**< ID of the area. */
	size_t size;			/**< Size of area. */
	mutex_t lock;			/**< Lock to protect area. */
	refcount_t count;		/**< Number of handles referring to the area. */
	khandle_t *source;		/**< Handle to source object. */
	offset_t offset;		/**< Offset into source. */
	avl_tree_t pages;		/**< Tree of pages for unbacked areas. */
} area_t;

/** Memory area ID allocator. */
static vmem_t *area_id_arena;

/** Slab cache for memory area structures. */
static slab_cache_t *area_cache;

/** Tree containing memory areas. */
static AVL_TREE_DECLARE(area_tree);
static RWLOCK_DECLARE(area_tree_lock);

/** Constructor for memory area structures.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void area_ctor(void *obj, void *data) {
	area_t *area = obj;

	mutex_init(&area->lock, "area_lock", 0);
	avl_tree_init(&area->pages);
}

/** Release a memory area.
 * @param area		Area to release. */
static void area_release(area_t *area) {
	vm_page_t *page;

	if(refcount_dec(&area->count) == 0) {
		/* Free all pages. */
		AVL_TREE_FOREACH_SAFE(&area->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			avl_tree_remove(&area->pages, page->offset);
			vm_page_free(page, 1);
		}

		rwlock_write_lock(&area_tree_lock);
		avl_tree_remove(&area_tree, area->id);
		rwlock_unlock(&area_tree_lock);

		if(area->source) {
			handle_release(area->source);
		}
		vmem_free(area_id_arena, area->id, 1);
		object_destroy(&area->obj);
		slab_cache_free(area_cache, area);
	}
}

/** Close a handle to a memory area.
 * @param handle	Handle to the area. */
static void area_object_close(khandle_t *handle) {
	area_release((area_t *)handle->object);
}

/** Get a page from the object.
 * @param handle	Handle to object to get page from.
 * @param offset	Offset into object to get page from.
 * @param physp		Where to store physical address of page.
 * @return		Status code describing result of the operation. */
static status_t area_object_get_page(khandle_t *handle, offset_t offset, phys_ptr_t *physp) {
	area_t *area = (area_t *)handle->object;
	status_t ret = STATUS_SUCCESS;
	vm_page_t *page;

	mutex_lock(&area->lock);

	/* Ensure that the requested page is within the area. */
	if(offset >= (offset_t)area->size) {
		mutex_unlock(&area->lock);
		return STATUS_INVALID_ADDR;
	}

	if(area->source) {
		/* Get the page from the source. */
		assert(area->source->object->type->get_page);
		ret = area->source->object->type->get_page(area->source, offset + area->offset, physp);
	} else {
		/* If the page is not already in the object, allocate a new page. */
		page = avl_tree_lookup(&area->pages, offset);
		if(!page) {
			page = vm_page_alloc(1, MM_SLEEP | PM_ZERO);
			page->offset = offset;
			avl_tree_insert(&area->pages, offset, page, NULL);
		}

		*physp = page->addr;
	}

	mutex_unlock(&area->lock);
	return ret;
}

/** Release a page from the object.
 * @param handle	Handle to object to release page in.
 * @param offset	Offset of page in object.
 * @param phys		Physical address of page that was unmapped. */
static void area_object_release_page(khandle_t *handle, offset_t offset, phys_ptr_t phys) {
	area_t *area = (area_t *)handle->object;

	if(area->source && area->source->object->type->release_page) {
		/* Release the page in the source. */
		area->source->object->type->release_page(area->source, offset + area->offset, phys);
	}
}

/** Memory area object type. */
static object_type_t area_object_type = {
	.id = OBJECT_TYPE_AREA,
	.close = area_object_close,
	.get_page = area_object_get_page,
	.release_page = area_object_release_page,
};

/** Create a new memory area.
 * @param size		Size of the area (multiple of system page size).
 * @param source	Handle to source object, or -1 if the area should be
 *			backed by anonymous memory.
 * @param offset	Offset within the source object to start from.
 * @param handlep	Where to store handle to area.
 * @return		Status code describing result of the operation. */
status_t sys_area_create(size_t size, handle_t source, offset_t offset, handle_t *handlep) {
	khandle_t *ksource = NULL;
	status_t ret;
	area_t *area;

	if(size == 0 || size % PAGE_SIZE || !handlep) {
		return STATUS_INVALID_ARG;
	}

	if(source >= 0) {
		/* FIXME: Need to call mappable(). */
		ret = handle_lookup(curr_proc, source, -1, &ksource);
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(!ksource->object->type->get_page) {
			return STATUS_NOT_SUPPORTED;
		}
	}

	area = slab_cache_alloc(area_cache, MM_SLEEP);
	area->id = vmem_alloc(area_id_arena, 1, 0);
	if(!area->id) {
		slab_cache_free(area_cache, area);
		if(ksource) {
			handle_release(ksource);
		}
		return STATUS_NO_AREAS;
	}
	object_init(&area->obj, &area_object_type);
	refcount_set(&area->count, 1);
	area->source = ksource;
	area->offset = offset;
	area->size = size;

	rwlock_write_lock(&area_tree_lock);
	avl_tree_insert(&area_tree, area->id, area, NULL);
	rwlock_unlock(&area_tree_lock);

	ret = handle_create_and_attach(curr_proc, &area->obj, NULL, 0, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		area_release(area);
	}
	return ret;
}

/** Open a handle to a memory area.
 * @param id		ID of area to open.
 * @param handlep	Where to store handle to area.
 * @return		Status code describing result of the operation. */
status_t sys_area_open(area_id_t id, handle_t *handlep) {
	status_t ret;
	area_t *area;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&area_tree_lock);

	area = avl_tree_lookup(&area_tree, id);
	if(!area) {
		rwlock_unlock(&area_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&area->count);
	rwlock_unlock(&area_tree_lock);

	ret = handle_create_and_attach(curr_proc, &area->obj, NULL, 0, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		area_release(area);
	}
	return ret;
}

/** Get the ID of a memory area.
 * @param handle	Handle to area.
 * @return		ID of area on success, -1 if handle is invalid. */
area_id_t sys_area_id(handle_t handle) {
	khandle_t *khandle;
	area_id_t ret;
	area_t *area;

	if(handle_lookup(curr_proc, handle, OBJECT_TYPE_AREA, &khandle) != STATUS_SUCCESS) {
		return -1;
	}

	area = (area_t *)khandle->object;
	ret = area->id;
	handle_release(khandle);
	return ret;
}

/** Get the size of a memory area.
 * @param handle	Handle to area.
 * @return		Size of area, or 0 if handle is invalid. */
size_t sys_area_size(handle_t handle) {
	khandle_t *khandle;
	area_t *area;
	size_t ret;

	if(handle_lookup(curr_proc, handle, OBJECT_TYPE_AREA, &khandle) != STATUS_SUCCESS) {
		return 0;
	}

	area = (area_t *)khandle->object;
	ret = area->size;
	handle_release(khandle);
	return ret;
}

/** Resize a memory area.
 * @todo		Support shrinking areas.
 * @param handle	Handle to area.
 * @param size		New size of the area.
 * @return		Status code describing result of the operation. */
status_t sys_area_resize(handle_t handle, size_t size) {
	khandle_t *khandle;
	status_t ret;
	area_t *area;

	if(size == 0 || size % PAGE_SIZE) {
		return STATUS_INVALID_ARG;
	}

	ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_AREA, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	area = (area_t *)khandle->object;
	if(size < area->size) {
		ret = STATUS_NOT_IMPLEMENTED;
	} else {
		area->size = size;
	}

	handle_release(khandle);
	return ret;
}

/** Initialise the memory area allocators. */
static void __init_text area_init(void) {
	area_id_arena = vmem_create("area_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	area_cache = slab_cache_create("area_cache", sizeof(area_t), 0, area_ctor,
	                              NULL, NULL, NULL, 0, MM_FATAL);
}
INITCALL(area_init);
