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
 * @brief		Kernel library header.
 */

#ifndef __LIBKERNEL_H
#define __LIBKERNEL_H

#include <kernel/process.h>
#include <kernel/status.h>

#include <util/list.h>

#include <elf.h>
#include <stdio.h>

/** Compiler attribute/builtin macros. */
#define __export		__attribute__((visibility("default")))
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

/** Round a value up. */
#define ROUND_UP(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
			__n += nearest; \
		} \
		__n; \
	})

/** Round a value down. */
#define ROUND_DOWN(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
		} \
		__n; \
	})

/** Size of the early heap. */
#define LIBKERNEL_HEAP_SIZE	8192

#include "arch.h"

/** Structure describing a loaded image. */
typedef struct rtld_image {
	list_t header;			/**< Link to loaded images library. */

	/** Basic image information. */
	const char *name;		/**< Shared object name of the library. */
	const char *path;		/**< Full path to image file. */
	int refcount;			/**< Reference count (tracks what is using the image). */
	elf_addr_t dynamic[ELF_DT_NUM];	/**< Dynamic section entries. */
	elf_dyn_t *dyntab;		/**< Pointer to unmodified dynamic section. */

	/** Where the image is loaded to (for ELF_ET_DYN). */
	void *load_base;		/**< Base address for the image. */
	size_t load_size;		/**< Size of the image's memory region. */

	/** Symbol hash table. */
	Elf32_Word *h_buckets;		/**< Hash table buckets. */
	int h_nbucket;			/**< Number of hash buckets. */
	Elf32_Word *h_chains;		/**< Hash table chains. */
	int h_nchain;			/**< Number of chain entries. */

	/** TLS information. */
	size_t tls_module_id;		/**< TLS module ID (0 if no TLS data). */
	void *tls_image;		/**< Initial TLS image. */
	size_t tls_filesz;		/**< File size of TLS image. */
	size_t tls_memsz;		/**< Memory size of TLS image. */
	size_t tls_align;		/**< TLS image alignment. */
	ptrdiff_t tls_offset;		/**< Offset of TLS data from thread pointer. */

	/** State of the image. */
	enum {
		RTLD_IMAGE_LOADING,	/**< Image is currently being loaded. */
		RTLD_IMAGE_LOADED,	/**< Image is fully loaded. */
	} state;
} rtld_image_t;

/** Heap operations structure. */
typedef struct libkernel_heap_ops {
	void *(*alloc)(size_t);
	void *(*realloc)(void *, size_t);
	void (*free)(void *);
} libkernel_heap_ops_t;

/** Pre-defined TLS module IDs. */
#define APPLICATION_TLS_ID	1	/**< Application always has module ID 1. */
#define LIBKERNEL_TLS_ID	2	/**< If libkernel has TLS, this will be its ID. */
#define DYNAMIC_TLS_START	2	/**< Start of dynamically allocated IDs. */

extern list_t loaded_images;
extern rtld_image_t libkernel_image;
extern rtld_image_t *application_image;

extern bool libkernel_debug;

/** Print a debug message. */
#define dprintf(fmt...)		if(libkernel_debug) { printf(fmt); }

extern status_t rtld_image_relocate(rtld_image_t *image);
extern status_t rtld_image_load(const char *path, rtld_image_t *req, int type, void **entryp,
                                rtld_image_t **imagep);
extern void rtld_image_unload(rtld_image_t *image);
extern bool rtld_symbol_lookup(rtld_image_t *start, const char *name, elf_addr_t *addrp,
                               rtld_image_t **sourcep);
extern void rtld_symbol_init(rtld_image_t *image);
extern void *rtld_init(process_args_t *args, bool dry_run);

extern size_t tls_alloc_module_id(void);
extern ptrdiff_t tls_tp_offset(rtld_image_t *image);
extern void *tls_get_addr(size_t module, size_t offset);
extern status_t tls_init(void);
extern void tls_destroy(void);

extern void libkernel_heap_configure(libkernel_heap_ops_t *ops);

extern void libkernel_arch_init(process_args_t *args, rtld_image_t *image);
extern void libkernel_init(process_args_t *args);

#endif /* __LIBKERNEL_H */
