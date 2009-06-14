/* Kiwi TLB invalidation functions
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
 * @brief		TLB invalidation functions.
 */

#ifndef __MM_TLB_H
#define __MM_TLB_H

#include <arch/tlb.h>

#include <mm/aspace.h>

extern void tlb_invalidate(aspace_t *as, ptr_t start, ptr_t end);

#endif /* __MM_TLB_H */
