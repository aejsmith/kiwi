/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		BIOS interrupt functions.
 */

#ifndef __PLATFORM_BIOS_H
#define __PLATFORM_BIOS_H

#include <lib/string.h>
#include <x86/cpu.h>
#include <types.h>

/** Memory area to use when passing data to BIOS interrupts (56KB).
 * @note		Area is actually 60KB, but the last 4KB are used for
 *			the stack. */
#define BIOS_MEM_BASE		0x1000
#define BIOS_MEM_SIZE		0xE000

/** Convert a segment + offset pair to a linear address. */
#define SEGOFF2LIN(segoff)	(ptr_t)((((segoff) & 0xFFFF0000) >> 12) + ((segoff) & 0xFFFF))

/** Convert a linear address to a segment + offset pair. */
#define LIN2SEGOFF(lin)		(uint32_t)(((lin & 0xFFFFFFF0) << 12) + (lin & 0xF))

/** Structure describing registers to pass to a BIOS interrupt. */
typedef struct bios_regs {
	uint32_t eflags, eax, ebx, ecx, edx, edi, esi, ebp, es;
} bios_regs_t;

/** Initialise a BIOS registers structure.
 * @param regs		Structure to initialise. */
static inline void bios_regs_init(bios_regs_t *regs) {
	memset(regs, 0, sizeof(bios_regs_t));
}

extern void bios_interrupt(uint8_t num, bios_regs_t *regs);

#endif /* __PLATFORM_BIOS_H */
