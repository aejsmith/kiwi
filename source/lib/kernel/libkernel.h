/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief		Kernel library header.
 */

#ifndef __LIBKERNEL_H
#define __LIBKERNEL_H

#include <kernel/private/process.h>
#include <kernel/private/thread.h>
#include <kernel/status.h>

#include <system/list.h>

#include <elf.h>
#include <stdio.h>

/** Compiler attribute/builtin macros. */
#define __export		__attribute__((visibility("default")))
#define __noreturn		__attribute__((noreturn))
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

/** Round a value up.
 * @param val		Value to round.
 * @param nearest	Boundary to round up to.
 * @return		Rounded value. */
#define round_up(val, nearest) \
	__extension__ \
	({ \
		typeof(val) __n = val; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
			__n += nearest; \
		} \
		__n; \
	})

/** Round a value down.
 * @param val		Value to round.
 * @param nearest	Boundary to round up to.
 * @return		Rounded value. */
#define round_down(val, nearest) \
	__extension__ \
	({ \
		typeof(val) __n = val; \
		if(__n % (nearest)) \
			__n -= __n % (nearest); \
		__n; \
	})

/** Get the lowest value out of a pair of values. */
#define min(a, b) \
	((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define max(a, b) \
	((a) < (b) ? (b) : (a))

#include "arch.h"

#if !defined(TLS_VARIANT_1) && !defined(TLS_VARIANT_2)
# error "Architecture must define TLS_VARIANT_1 or TLS_VARIANT_2"
#endif

/** Structure describing a loaded image. */
typedef struct rtld_image {
	sys_list_t header;		/**< Link to loaded images library. */

	/** Basic image information. */
	image_id_t id;			/**< ID of the image. */
	const char *name;		/**< Shared object name of the library. */
	const char *path;		/**< Full path to image file. */
	node_id_t node;			/**< Node ID of image. */
	mount_id_t mount;		/**< Mount that the node is on. */
	int refcount;			/**< Reference count (tracks what is using the image). */
	elf_ehdr_t *ehdr;		/**< ELF executable header. */
	elf_phdr_t *phdrs;		/**< Address of program headers. */
	size_t num_phdrs;		/**< Number of program headers. */
	elf_addr_t dynamic[ELF_DT_NUM];	/**< Cached dynamic section entries. */
	elf_dyn_t *dyntab;		/**< Pointer to dynamic section. */

	/** Where the image is loaded to (for ELF_ET_DYN). */
	void *load_base;		/**< Base address for the image. */
	size_t load_size;		/**< Size of the image's memory region. */

	/** Symbol hash table. */
	Elf32_Word *h_buckets;		/**< Hash table buckets. */
	int h_nbucket;			/**< Number of hash buckets. */
	Elf32_Word *h_chains;		/**< Hash table chains. */
	int h_nchain;			/**< Number of chain entries. */

	/** TLS information. */
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

/** Structure giving symbol information. */
typedef struct rtld_symbol {
	elf_addr_t addr;		/**< Symbol address. */
	rtld_image_t *image;		/**< Image containing symbol. */
} rtld_symbol_t;

/** Pre-defined image IDs. */
#define APPLICATION_IMAGE_ID	1	/**< Application always has module ID 1. */
#define LIBKERNEL_IMAGE_ID	2	/**< If libkernel has TLS, this will be its ID. */
#define DYNAMIC_IMAGE_START	3	/**< Start of dynamically allocated IDs. */

extern elf_dyn_t _DYNAMIC[];
extern char _end[];

extern image_id_t next_image_id;
extern sys_list_t loaded_images;
extern rtld_image_t libkernel_image;
extern rtld_image_t *application_image;

extern __thread thread_id_t curr_thread_id;
extern process_id_t curr_process_id;
extern size_t page_size;

extern bool libkernel_debug;
extern bool libkernel_dry_run;

/** Print a debug message. */
#define dprintf(fmt...)		if(libkernel_debug) { printf(fmt); }

extern status_t arch_rtld_image_relocate(rtld_image_t *image);

extern rtld_image_t *rtld_image_lookup(image_id_t id);
extern bool rtld_symbol_lookup(rtld_image_t *start, const char *name,
	rtld_symbol_t *symbol);
extern void rtld_symbol_init(rtld_image_t *image);
extern status_t rtld_image_load(const char *name, rtld_image_t *req,
	rtld_image_t **imagep);
extern status_t rtld_init(process_args_t *args, void **entryp);

extern ptrdiff_t tls_tp_offset(rtld_image_t *image);
extern void *tls_get_addr(size_t module, size_t offset);
extern status_t tls_alloc(tls_tcb_t **tcbp);
extern void tls_destroy(tls_tcb_t *tcb);

extern void libkernel_init(process_args_t *args) __noreturn;
extern void libkernel_abort(void) __noreturn;

#endif /* __LIBKERNEL_H */
