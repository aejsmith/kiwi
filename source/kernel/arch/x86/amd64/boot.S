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
 * @brief		AMD64 boot code.
 */

#include <arch/page.h>

.section .data, "aw", @progbits

/** Kernel PDP. */
.align PAGE_SIZE
.global __kernel_pdp
.type __kernel_pdp, @object
__kernel_pdp:
	.fill	512, 8, 0
.size __kernel_pdp, .-__kernel_pdp

/** Initial PML4. */
.align PAGE_SIZE
.global __boot_pml4
.type __boot_pml4, @object
__boot_pml4:
	.fill	512, 8, 0
.size __boot_pml4, .-__boot_pml4
