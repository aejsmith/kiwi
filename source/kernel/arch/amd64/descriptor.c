/* Kiwi AMD64 descriptor table functions
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
 * @brief		AMD64 descriptor table functions.
 */

#include <arch/descriptor.h>
#include <arch/memmap.h>
#include <arch/x86/fault.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/irq.h>

#include <lib/string.h>

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t __isr_array[IDT_ENTRY_COUNT][16];

/** Array of GDT descriptors. */
static gdt_entry_t __initial_gdt[] __aligned(8) = {
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

/** Array of IDT entries. */
static idt_entry_t idt[IDT_ENTRY_COUNT] __aligned(8);

/** Double fault handler stack. */
static uint8_t __doublefault_stack[KSTACK_SIZE] __aligned(PAGE_SIZE);

/** Bootstrap GDT pointer. */
gdt_pointer_t __boot_gdtp = {
	.limit = sizeof(__initial_gdt) - 1,
	.base = (ptr_t)KA2PA((ptr_t)__initial_gdt),
};

/** Set up the GDT for the current CPU. */
static void gdt_init(void) {
	gdt_tss_entry_t *desc;
	size_t size;
	ptr_t base;

	/* Create a copy of the statically allocated GDT. */
	memcpy(curr_cpu->arch.gdt, __initial_gdt, sizeof(__initial_gdt));

	/* Set up the TSS descriptor. */
	base = (ptr_t)&curr_cpu->arch.tss;
	size = sizeof(tss_t);

	desc = (gdt_tss_entry_t *)&curr_cpu->arch.gdt[SEG_TSS / 0x08];
	desc->base0 = base & 0xffff;
	desc->base1 = ((base) >> 16) & 0xff;
	desc->base2 = ((base) >> 24) & 0xff;
	desc->base3 = ((base) >> 32);
	desc->limit0 = size & 0xffff;
	desc->limit1 = (size >> 16) & 0xf;
	desc->present = 1;
	desc->type = 0x9;

	/* Set the GDT pointer. */
	lgdt((ptr_t)&curr_cpu->arch.gdt, sizeof(curr_cpu->arch.gdt) - 1);
}

/** Set up the TSS for the current CPU. */
static void tss_init(void) {
	/* Set up the contents of the TSS. Point the first IST entry at the
	 * double fault stack. */
	memset(&curr_cpu->arch.tss, 0, sizeof(tss_t));
	curr_cpu->arch.tss.ist1 = ((ptr_t)__doublefault_stack + KSTACK_SIZE) - STACK_DELTA;
	curr_cpu->arch.tss.io_bitmap = 104;

	/* Load the TSS segment into TR. */
	ltr(SEG_TSS);
}

/** Initialize the IDT shared by all CPUs. */
static void idt_init(void) {
	unative_t i;
	ptr_t addr;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		addr = (ptr_t)&__isr_array[i];
		idt[i].base0 = (addr & 0xFFFF);
		idt[i].base1 = ((addr >> 16) & 0xFFFF);
		idt[i].base2 = ((addr >> 32) & 0xFFFFFFFF);
		idt[i].ist = 0;
		idt[i].reserved = 0;
		idt[i].sel = SEG_K_CS;
		idt[i].unused = 0;
		idt[i].flags = 0x8E;
	}

	/* In tss_init() above we point the first IST entry at the double
	 * fault stack. Point the double fault IDT entry at this stack. */
	idt[FAULT_DOUBLE].ist = 1;

	/* Now we can fill out the interrupt handler table. Entries 0-31 are
	 * exceptions. */
	for(i = 0; i < 32; i++) {
		intr_register(i, fault_handler);
	}

	/* Entries 32-47 are IRQs, 48 onwards are unrecognised for now. */
	for(i = 32; i <= 47; i++) {
		intr_register(i, irq_handler);
	}
}

/** Initialize descriptor tables for the boot CPU. */
void descriptor_init(void) {
	gdt_init();
	tss_init();

	/* The IDT only needs to be initialized once. Do that now as we are on
	 * the boot CPU. */
	idt_init();

	/* Point the CPU to the new IDT. */
	lidt((ptr_t)&idt, (sizeof(idt) - 1));
}

/** Initialize descriptor tables for an application CPU. */
void descriptor_ap_init(void) {
	/* The GDT/TSS setup procedures are the same on both the BSP and APs,
	 * so just call the functions for them. */
	gdt_init();
	tss_init();

	/* For the IDT, there is no need to have a seperate IDT for each CPU,
	 * so just point the IDTR at the shared IDT. */
	lidt((ptr_t)&idt, (sizeof(idt) - 1));
}
