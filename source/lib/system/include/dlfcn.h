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
 * @brief		Dynamic linking functions.
 */

#ifndef __DLFCN_H
#define __DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

/** Mode flags for dlopen(). */
#define RTLD_LAZY	(1<<0)		/**< Relocations are performed as needed. */
#define RTLD_NOW	(1<<1)		/**< Relocations are performed immediately. */
#define RTLD_GLOBAL	(1<<2)		/**< Module is available for linking to other modules. */
#define RTLD_LOCAL	(1<<3)		/**< Module is not available for linking to other modules. */

/* extern int dlclose(void *); */
/* extern char *dlerror(void); */
/* extern void *dlopen(const char *, int); */
/* extern void *dlsym(void *restrict, const char *restrict); */

/** Symbol information. */
typedef struct dl_info {
	const char *dli_fname;		/**< Pathname of shared object. */
	void *dli_fbase;		/**< Address at which shared object is loaded. */
	const char *dli_sname;		/**< Name of nearest symbol. */
	void *dli_saddr;		/**< Exact address of named symbol. */
} Dl_info;

static inline int dladdr(void *addr, Dl_info *info) { return 0; }

#ifdef __cplusplus
}
#endif

#endif /* __DLFCN_H */
