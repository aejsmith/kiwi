/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		AMD64 MMU context definitions.
 */

#ifndef __X86_MMU_H
#define __X86_MMU_H

/** Definitions of paging structure bits. */
#define X86_PTE_PRESENT		(1<<0)		/**< Page is present. */
#define X86_PTE_WRITE		(1<<1)		/**< Page is writable. */
#define X86_PTE_USER		(1<<2)		/**< Page is accessible in CPL3. */
#define X86_PTE_PWT		(1<<3)		/**< Page has write-through caching. */
#define X86_PTE_PCD		(1<<4)		/**< Page has caching disabled. */
#define X86_PTE_ACCESSED	(1<<5)		/**< Page has been accessed. */
#define X86_PTE_DIRTY		(1<<6)		/**< Page has been written to. */
#define X86_PTE_LARGE		(1<<7)		/**< Page is a large page. */
#define X86_PTE_GLOBAL		(1<<8)		/**< Page won't be cleared in TLB. */
#ifndef __ASM__
# define X86_PTE_NOEXEC		(1LL<<63)	/**< Page is not executable (requires NX support). */
#else
# define X86_PTE_NOEXEC		(1<<63)		/**< Page is not executable (requires NX support). */
#endif

/** Protection flag mask. */
#define X86_PTE_PROTECT_MASK	(X86_PTE_WRITE | X86_PTE_NOEXEC)

/** Cacheability flag mask. */
#define X86_PTE_CACHE_MASK	(X86_PTE_PWT | X86_PTE_PCD)

#ifndef __ASM__

extern phys_ptr_t ap_bootstrap_page;

#endif /* __ASM__ */
#endif /* __X86_MMU_H */
