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
 * @brief		PXE loader wrapper.
 *
 * There is supposedly a 32KB limit on the boot file size. However, gPXE and
 * none of my real hardware actually imposes this limit. Therefore, I'm
 * ignoring it. If we encounter any hardware that actually has this limit,
 * this will have to be done a different way...
 */

#include <arch/asm.h>
#include <platform/boot.h>

.section .text
.code16

/** Main function of the PXE boot sector. */
FUNCTION_START(_start)
	/* Set the boot drive ID to 0x7F for PXE. */
	movb	$0x7F, %dl
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx

	/* Jump to the loader entry point. */
	ljmp	$(LOADER_LOAD_ADDR >> 4), $0
FUNCTION_END(_start)

/** Pad the file to the loader load address. */
. = _start + LOADER_LOAD_ADDR - 0x7C00 - 1;
.byte 0
