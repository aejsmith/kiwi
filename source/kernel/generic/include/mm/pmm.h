/* Kiwi physical memory manager
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
 * @brief		Physical memory manager.
 */

#ifndef __MM_PMM_H
#define __MM_PMM_H

#include <arch/page.h>

#include <mm/flags.h>

/** Flags to modify allocation behaviour. */
#define PM_ZERO		(1<<10)		/**< Clear the page contents before returning. */

extern phys_ptr_t pmm_alloc(size_t count, int pmflag);
extern void pmm_free(phys_ptr_t base, size_t count);

extern void pmm_add(phys_ptr_t start, phys_ptr_t end);
extern void pmm_mark_reclaimable(phys_ptr_t start, phys_ptr_t end);
extern void pmm_mark_reserved(phys_ptr_t start, phys_ptr_t end);

extern void pmm_init(void *data);
extern void pmm_init_reclaim(void);

#endif /* __MM_PMM_H */
