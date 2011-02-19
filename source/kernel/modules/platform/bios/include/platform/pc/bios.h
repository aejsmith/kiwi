/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		PC BIOS interrupt interface.
 */

#ifndef __PLATFORM_PC_BIOS_H
#define __PLATFORM_PC_BIOS_H

#include <arch/x86/cpu.h>
#include <mm/flags.h>

/** Convert a segment + offset pair to a linear address. */
#define SEGOFF2LIN(segoff)	((ptr_t)(((segoff) & 0xFFFF0000) >> 12) + ((segoff) & 0xFFFF))

/** Structure describing registers to pass to a BIOS interrupt. */
typedef struct bios_regs {
	uint32_t eax, ebx, ecx, edx, edi, esi, ebp, eflags;
	uint32_t ds, es, fs, gs;
} bios_regs_t;

extern void *bios_mem_alloc(size_t size, int mmflag);
extern void bios_mem_free(void *addr, size_t size);
extern uint32_t bios_mem_virt2phys(void *addr);
extern void *bios_mem_phys2virt(uint32_t addr);

extern void bios_regs_init(bios_regs_t *regs);

extern void bios_interrupt(uint8_t num, bios_regs_t *regs);

#endif /* __PLATFORM_PC_BIOS_H */
