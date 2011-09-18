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

#include <arch/io.h>

#include <mm/heap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <pc/bios.h>

#include <sync/mutex.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <module.h>
#include <status.h>

#include "x86emu/x86emu.h"

/** BIOS memory area definitions. */
#define BIOS_BDA_BASE		0
#define BIOS_BDA_SIZE		0x1000
#define BIOS_EBDA_BASE		0x90000
#define BIOS_EBDA_SIZE		0x70000
#define BIOS_MEM_BASE		0x1000
#define BIOS_MEM_SIZE		0x8F000
#define BIOS_STACK_SIZE		0x1000

/** BIOS memory allocation data. */
static void *bios_mem_mapping = NULL;
static phys_ptr_t bios_mem_pages = 0;

/** Lock to serialise BIOS interrupt calls. */
static MUTEX_DECLARE(bios_lock, MUTEX_RECURSIVE);

/** Read 8 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint8_t x86emu_pio_inb(uint16_t port) {
	return in8(port);
}

/** Write 8 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outb(uint16_t port, uint8_t data) {
	out8(port, data);
}

/** Read 16 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint16_t x86emu_pio_inw(uint16_t port) {
	return in16(port);
}

/** Write 16 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outw(uint16_t port, uint16_t data) {
	out16(port, data);
}

/** Read 32 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint32_t x86emu_pio_inl(uint16_t port) {
	return in32(port);
}

/** Write 32 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outl(uint16_t port, uint32_t data) {
	out32(port, data);
}

/** X86EMU PIO helpers. */
static X86EMU_pioFuncs x86emu_pio_funcs = {
	.inb  = x86emu_pio_inb,
	.outb = x86emu_pio_outb,
	.inw  = x86emu_pio_inw,
	.outw = x86emu_pio_outw,
	.inl  = x86emu_pio_inl,
	.outl = x86emu_pio_outl,
};

/** Read an 8-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint8_t x86emu_mem_rdb(uint32_t addr) {
	return *(uint8_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping);
}

/** Write an 8-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrb(uint32_t addr, uint8_t val) {
	*(uint8_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping) = val;
}

/** Read a 16-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint16_t x86emu_mem_rdw(uint32_t addr) {
	return *(uint16_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping);
}

/** Write a 16-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrw(uint32_t addr, uint16_t val) {
	*(uint16_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping) = val;
}

/** Read a 32-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint32_t x86emu_mem_rdl(uint32_t addr) {
	return *(uint32_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping);
}

/** Write a 32-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrl(uint32_t addr, uint32_t val) {
	*(uint32_t *)((ptr_t)addr + (ptr_t)bios_mem_mapping) = val;
}

/** X86EMU memory helpers. */
static X86EMU_memFuncs x86emu_mem_funcs = {
	.rdb = x86emu_mem_rdb,
	.wrb = x86emu_mem_wrb,
	.rdw = x86emu_mem_rdw,
	.wrw = x86emu_mem_wrw,
	.rdl = x86emu_mem_rdl,
	.wrl = x86emu_mem_wrl,
};

static uint32_t next_alloc = BIOS_MEM_BASE;
static size_t remaining_size = BIOS_MEM_SIZE;

/** Allocate space in the BIOS memory area.
 * @param size		Size of allocation.
 * @param mmflag	Allocation flags.
 * @return		Pointer to kernel address of allocation on success,
 *			NULL on failure. The returned addresss must not be
 *			passed to interrupts - use bios_mem_virt2phys() to
 *			convert the address to a physical address first. */
void *bios_mem_alloc(size_t size, int mmflag) {
	uint32_t ret;

	mutex_lock(&bios_lock);

	if(size > remaining_size) {
		if(mmflag & MM_SLEEP) {
			fatal("oh dear\n");
		}
		return NULL;
	}

	ret = next_alloc;
	next_alloc += size;
	remaining_size -= size;

	mutex_unlock(&bios_lock);
	return bios_mem_phys2virt(ret);
}
MODULE_EXPORT(bios_mem_alloc);

/** Free space from the BIOS memory area.
 * @param addr		Address of allocation.
 * @param size		Size of the original allocation. */
void bios_mem_free(void *addr, size_t size) {

}
MODULE_EXPORT(bios_mem_free);

/** Convert a virtual address to a physical address.
 *
 * Converts the virtual address of part of the BIOS memory area to a physical
 * address that can be passed to BIOS interrupts.
 *
 * @param addr		Virtual address.
 *
 * @return		Address that can be used by BIOS code.
 */
uint32_t bios_mem_virt2phys(void *addr) {
	assert(addr >= bios_mem_mapping);
	assert((ptr_t)addr < ((ptr_t)bios_mem_mapping + 0x100000));

	return (uint32_t)((ptr_t)addr - (ptr_t)bios_mem_mapping);
}
MODULE_EXPORT(bios_mem_virt2phys);

/** Convert a BIOS memory address to a virtual address.
 *
 * Converts a physical BIOS memory address to a virtual kernel address. Be
 * aware of data that may straddle across a boundary between different memory
 * areas.
 *
 * @param addr		Physical address.
 *
 * @return		Virtual address, or NULL if address was invalid.
 */
void *bios_mem_phys2virt(uint32_t addr) {
	assert(addr < 0x100000);

	return (void *)((ptr_t)addr + (ptr_t)bios_mem_mapping);
}
MODULE_EXPORT(bios_mem_phys2virt);

/** Execute a BIOS interrupt.
 *
 * Executes a real-mode BIOS interrupt. Calls to BIOS interrupts are
 * serialized.
 *
 * @param num		Interrupt number to call.
 * @param regs		Registers to pass to interrupt. Will be updated with
 *			the register state after calling the interrupt.
 */
void bios_interrupt(uint8_t num, bios_regs_t *regs) {
	void *halt, *stack;

	assert(regs);

	mutex_lock(&bios_lock);

	uint32_t save_next_alloc = next_alloc;
	size_t save_remaining_size = remaining_size;

	/* Allocate a stack and a halt byte to finish execution. */
	stack = bios_mem_alloc(BIOS_STACK_SIZE, MM_SLEEP);
	halt = bios_mem_alloc(1, MM_SLEEP);
	*(uint8_t *)halt = 0xF4;

	/* Copy in the registers. */
	memset(&M, 0, sizeof(M));
	M.x86.R_EAX = regs->eax;
	M.x86.R_EBX = regs->ebx;
	M.x86.R_ECX = regs->ecx;
	M.x86.R_EDX = regs->edx;
	M.x86.R_EDI = regs->edi;
	M.x86.R_ESI = regs->esi;
	M.x86.R_EBP = regs->ebp;
	M.x86.R_ESP = bios_mem_virt2phys(stack + BIOS_STACK_SIZE);
	M.x86.R_EFLG = regs->eflags;
	M.x86.R_EIP = bios_mem_virt2phys(halt);
	M.x86.R_CS = 0x0;
	M.x86.R_DS = regs->ds;
	M.x86.R_ES = regs->es;
	M.x86.R_FS = regs->fs;
	M.x86.R_GS = regs->gs;
	M.x86.R_SS = 0x0;

	/* Run the interrupt. */
	X86EMU_prepareForInt(num);
	X86EMU_exec();

	/* Copy back modified registers. */
	regs->eax = M.x86.R_EAX;
	regs->ebx = M.x86.R_EBX;
	regs->ecx = M.x86.R_ECX;
	regs->edx = M.x86.R_EDX;
	regs->edi = M.x86.R_EDI;
	regs->esi = M.x86.R_ESI;
	regs->ebp = M.x86.R_EBP;
	regs->eflags = M.x86.R_EFLG;
	regs->ds = M.x86.R_DS;
	regs->es = M.x86.R_ES;
	regs->fs = M.x86.R_FS;
	regs->gs = M.x86.R_GS;

	/* Free up data. */
	bios_mem_free(halt, 1);
	bios_mem_free(stack, BIOS_STACK_SIZE);
	next_alloc = save_next_alloc;
	remaining_size = save_remaining_size;

	mutex_unlock(&bios_lock);
}
MODULE_EXPORT(bios_interrupt);

/** Initialise a registers structure.
 * @param regs		Structure to initialise. */
void bios_regs_init(bios_regs_t *regs) {
	memset(regs, 0, sizeof(bios_regs_t));
	regs->eflags = X86_FLAGS_IF | X86_FLAGS_ALWAYS1;
}
MODULE_EXPORT(bios_regs_init);

/** Map a range into the BIOS memory mapping.
 * @param addr		Address within the mapping to map.
 * @param phys		Physical address to map.
 * @param size		Size to map. */
static void bios_mem_map(ptr_t addr, phys_ptr_t phys, size_t size) {
	ptr_t i;

	for(i = 0; i < size; i += PAGE_SIZE) {
		page_map_lock(&kernel_page_map);
		page_map_insert(&kernel_page_map, (ptr_t)bios_mem_mapping + addr + i,
		                phys + i, true, true, MM_SLEEP);
		page_map_unlock(&kernel_page_map);
	}
}

/** Initialisation function for the BIOS module.
 * @return		Status code describing result of the operation. */
static status_t bios_init(void) {
	/* Allocate a chunk of heap space and map stuff into it. */
	bios_mem_mapping = (void *)heap_raw_alloc(0x100000, MM_SLEEP);
	phys_alloc(BIOS_MEM_SIZE, 0, 0, 0, 0, MM_SLEEP, &bios_mem_pages);
	bios_mem_map(BIOS_BDA_BASE, BIOS_BDA_BASE, BIOS_BDA_SIZE);
	bios_mem_map(BIOS_MEM_BASE, bios_mem_pages, BIOS_MEM_SIZE);
	bios_mem_map(BIOS_EBDA_BASE, BIOS_EBDA_BASE, BIOS_EBDA_SIZE);

	/* Initialise the I/O and memory functions for X86EMU. */
	X86EMU_setupPioFuncs(&x86emu_pio_funcs);
	X86EMU_setupMemFuncs(&x86emu_mem_funcs);
	return STATUS_SUCCESS;
}

/** Unloading function for the BIOS module.
 * @return		Status code describing result of the operation. */
static status_t bios_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("bios");
MODULE_DESC("PC BIOS interrupt interface");
MODULE_FUNCS(bios_init, bios_unload);
