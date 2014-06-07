/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Dynamic linker interface.
 */

#ifndef __LINK_H
#define __LINK_H

#include <system/arch/types.h>

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _GNU_SOURCE

#define ElfW(type)		_ElfW(Elf, __WORDSIZE, type)
#define _ElfW(e,w,t)		_ElfW_1(e, w, _##t)
#define _ElfW_1(e,w,t)		e##w##t

/** Structure containing information about a loaded shared object. */
struct dl_phdr_info {
	ElfW(Addr) dlpi_addr;		/**< Base address of object. */
	const char *dlpi_name;		/**< Name of object. */
	const ElfW(Phdr) *dlpi_phdr;	/**< Pointer to array of program headers. */
	ElfW(Half) dlpi_phnum;		/**< Number of program headers. */
};

extern int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *),
	void *data);

#endif /* _GNU_SOURCE */

#ifdef __cplusplus
}
#endif

#endif /* __LINK_H */
