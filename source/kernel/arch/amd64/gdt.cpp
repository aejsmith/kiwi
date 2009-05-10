/* Kiwi AMD64 GDT functions
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		AMD64 GDT functions.
 */

#include <arch/asm.h>
//#include <arch/gdt.h>
#include <arch/mem.h>
#include <arch/segment.h>

//#include <cpu/cpu.h>

//#include <lib/string.h>

/** Array of GDT descriptors. */
static gdt_desc_t __initial_gdt[] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/**< NULL descriptor. */
	{ 0xFFFF, 0, 0, 0x9A, 0xF, 0, 1, 0, 1, 0 },	/**< Kernel CS (Code). */
	{ 0xFFFF, 0, 0, 0x92, 0xF, 0, 0, 0, 1, 0 },	/**< Kernel DS (Data). */
	{ 0xFFFF, 0, 0, 0xF2, 0xF, 0, 0, 1, 1, 0 },	/**< User DS (Data). */
	{ 0xFFFF, 0, 0, 0xF8, 0xF, 0, 1, 0, 1, 0 },	/**< User CS (Code). */
	{ 0xFFFF, 0, 0, 0x9A, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel 32-bit CS (Code). */
	{ 0xFFFF, 0, 0, 0x92, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel 32-bit DS (Data). */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/**< TSS descriptor - filled in by gdt_init(). */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/**< Second part of TSS descriptor. */
};

/** Bootstrap GDT pointer. */
gdt_ptr_t __boot_gdtp = {
	sizeof(__initial_gdt) - 1,
	(ptr_t)KA2PA((ptr_t)__initial_gdt),
};

#if 0
/** Double fault handler stack. */
static uint8_t __doublefault_stack[KSTACK_SIZE] __aligned(PAGE_SIZE);

/** Set the base address of a segment.
 *
 * Modifies the base address of a segment in the GDT.
 *
 * @param sel		Segment to modify.
 * @param base		New base address of the segment.
 */
void gdt_set_base(int sel, ptr_t base) {
	curr_cpu->arch.gdt[sel / 0x08].base0 = (base & 0xFFFF);
	curr_cpu->arch.gdt[sel / 0x08].base1 = (base >> 16) & 0xFF;
	curr_cpu->arch.gdt[sel / 0x08].base2 = (base >> 24) & 0xFF;
}

/** Set the limit of a segment.
 *
 * Modifies the limit of a segment in the GDT.
 *
 * @param sel		Segment to modify.
 * @param limit		New limit of the segment.
 */
void gdt_set_limit(int sel, size_t limit) {
	curr_cpu->arch.gdt[sel / 0x08].limit0 = (limit & 0xFFFF);
	curr_cpu->arch.gdt[sel / 0x08].limit1 = (limit >> 16) & 0xF;
}

/** Set up the GDT/TSS for the current CPU. */
void gdt_init(void) {
	gdt_tss_desc_t *desc;
	size_t size;
	ptr_t base;

	/* Create a copy of the statically allocated GDT. */
	memcpy(curr_cpu->arch.gdt, __initial_gdt, sizeof(__initial_gdt));

	/* Set up the TSS descriptor. */
	base = (ptr_t)&curr_cpu->arch.tss;
	size = sizeof(tss_t);

	desc = (gdt_tss_desc_t *)&curr_cpu->arch.gdt[SEG_TSS / 0x08];
	desc->base0 = base & 0xffff;
	desc->base1 = ((base) >> 16) & 0xff;
	desc->base2 = ((base) >> 24) & 0xff;
	desc->base3 = ((base) >> 32);
	desc->limit0 = size & 0xffff;
	desc->limit1 = (size >> 16) & 0xf;
	desc->present = 1;
	desc->type = 0x9;

	/* Set up the contents of the TSS. */
	memset(&curr_cpu->arch.tss, 0, sizeof(tss_t));
	curr_cpu->arch.tss.ist1 = ((ptr_t)__doublefault_stack + KSTACK_SIZE) - STACK_DELTA;
	curr_cpu->arch.tss.io_bitmap = 104;

	/* Set the GDT pointer and load the TSS descriptor. */
	lgdt((ptr_t)&curr_cpu->arch.gdt, sizeof(curr_cpu->arch.gdt) - 1);
	ltr(SEG_TSS);
}
#endif
