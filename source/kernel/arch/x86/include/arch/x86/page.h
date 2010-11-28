/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 paging definitions.
 */

#ifndef __ARCH_X86_PAGE_H
#define __ARCH_X86_PAGE_H

/** Definitions of paging structure bits. */
#define PG_PRESENT		(1<<0)		/**< Page is present. */
#define PG_WRITE		(1<<1)		/**< Page is writable. */
#define PG_USER			(1<<2)		/**< Page is accessible in CPL3. */
#define PG_PWT			(1<<3)		/**< Page has write-through caching. */
#define PG_PCD			(1<<4)		/**< Page has caching disabled. */
#define PG_ACCESSED		(1<<5)		/**< Page has been accessed. */
#define PG_DIRTY		(1<<6)		/**< Page has been written to. */
#define PG_LARGE		(1<<7)		/**< Page is a large page. */
#define PG_GLOBAL		(1<<8)		/**< Page won't be cleared in TLB. */
#ifndef __ASM__
# define PG_NOEXEC		(1LL<<63)	/**< Page is not executable (requires NX support). */
#else
# define PG_NOEXEC		(1<<63)		/**< Page is not executable (requires NX support). */
#endif

#if !defined(__ASM__) && !defined(LOADER)

extern void pat_init(void);

#endif /* __ASM__/LOADER */
#endif /* __ARCH_X86_PAGE_H */
