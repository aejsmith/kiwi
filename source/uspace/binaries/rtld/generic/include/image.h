/*
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
 * @brief		RTLD image management.
 */

#ifndef __RTLD_IMAGE_H
#define __RTLD_IMAGE_H

#include <rtld/arch/elf.h>
#include <rtld/list.h>

#define __need_size_t
#define __need_NULL
#include <stddef.h>

struct process_args;

/** Structure describing a loaded image. */
typedef struct rtld_image {
	list_t header;			/**< Link to loaded images library. */

	const char *name;		/**< Shared object name of the library. */
	char *path;			/**< Full path to image file. */
	int refcount;			/**< Reference count (tracks what is using the image). */
	ElfW(Addr) dynamic[ELF_DT_NUM];	/**< Dynamic section entries. */
	ElfW(Dyn) *dyntab;		/**< Pointer to unmodified dynamic section. */

	void *load_base;		/**< Base address for the image. */
	size_t load_size;		/**< Size of the image's memory region. */

	Elf32_Word *h_buckets;		/**< Hash table buckets. */
	int h_nbucket;			/**< Number of hash buckets. */
	Elf32_Word *h_chains;		/**< Hash table chains. */
	int h_nchain;			/**< Number of chain entries. */

	/** State of the image. */
	enum {
		RTLD_IMAGE_LOADING,	/**< Image is currently being loaded. */
		RTLD_IMAGE_LOADED,	/**< Image is fully loaded. */
	} state;
} rtld_image_t;

extern list_t rtld_loaded_images;
extern rtld_image_t *rtld_application;

extern int rtld_image_relocate(rtld_image_t *image);

extern int rtld_image_load(const char *path, rtld_image_t *req, int type, void **entryp, rtld_image_t **imagep);
extern void rtld_image_unload(rtld_image_t *image);
extern int rtld_library_load(const char *name, rtld_image_t *req, rtld_image_t **imagep);

#endif /* __RTLD_IMAGE_H */
