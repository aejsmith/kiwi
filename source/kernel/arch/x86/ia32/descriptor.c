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
 * @brief		IA32 descriptor table functions.
 */

#include <arch/descriptor.h>
#include <arch/memmap.h>
#include <arch/page.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

extern void syscall_entry(void);

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Array of GDT descriptors. */
static gdt_entry_t initial_gdt[] __aligned(8) = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/**< NULL descriptor. */
	{ 0xFFFF, 0, 0, 0x9A, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel CS (Code). */
	{ 0xFFFF, 0, 0, 0x92, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel DS (Data). */
	{ 0xFFFF, 0, 0, 0xFE, 0xF, 0, 0, 1, 1, 0 },	/**< User CS (Code). */
	{ 0xFFFF, 0, 0, 0xF2, 0xF, 0, 0, 1, 1, 0 },	/**< User DS (Data). */
	{ 0xFFFF, 0, 0, 0xF2, 0xF, 0, 0, 1, 1, 0 },	/**< User GS (TLS). */
	{ 0, 0, 0, 0x89, 0, 0, 0, 1, 0, 0 },		/**< TSS descriptor. */
	{ 0, 0, 0, 0x89, 0, 0, 0, 1, 0, 0 },		/**< Doublefault TSS descriptor. */
};

/** Array of IDT entries. */
static idt_entry_t kernel_idt[IDT_ENTRY_COUNT] __aligned(8);

/** Set the base address of a segment.
 * @param sel		Segment to modify.
 * @param base		New base address of the segment. */
void gdt_set_base(int sel, ptr_t base) {
	curr_cpu->arch.gdt[sel / 0x08].base0 = (base & 0xFFFF);
	curr_cpu->arch.gdt[sel / 0x08].base1 = (base >> 16) & 0xFF;
	curr_cpu->arch.gdt[sel / 0x08].base2 = (base >> 24) & 0xFF;
}

/** Set the limit of a segment.
 * @param sel		Segment to modify.
 * @param limit		New limit of the segment. */
static inline void gdt_set_limit(int sel, size_t limit) {
	curr_cpu->arch.gdt[sel / 0x08].limit0 = (limit & 0xFFFF);
	curr_cpu->arch.gdt[sel / 0x08].limit1 = (limit >> 16) & 0xF;
}

/** Set up the GDT for the current CPU. */
static void __init_text gdt_init(void) {
	/* Create a copy of the statically allocated GDT. */
	memcpy(curr_cpu->arch.gdt, initial_gdt, sizeof(initial_gdt));

	/* Set up the TSS descriptor. */
	gdt_set_base(SEGMENT_TSS, (ptr_t)&curr_cpu->arch.tss);
	gdt_set_limit(SEGMENT_TSS, sizeof(tss_t));
	gdt_set_base(SEGMENT_DF_TSS, (ptr_t)&curr_cpu->arch.double_fault_tss);
	gdt_set_limit(SEGMENT_DF_TSS, sizeof(tss_t));

	/* Set the GDT pointer. */
	lgdt((ptr_t)&curr_cpu->arch.gdt, sizeof(curr_cpu->arch.gdt) - 1);

	/* Reload the segment registers. */
	__asm__ volatile(
		"ljmp	%0, $1f\n"
		"1:\n"
		"mov	%1, %%ds\n"
		"mov	%1, %%es\n"
		"mov	%1, %%fs\n"
		"mov	%1, %%gs\n"
		"mov	%1, %%ss\n"
		:: "i"(SEGMENT_K_CS), "r"(SEGMENT_K_DS)
	);
}

/** Set up the TSS for the current CPU. */
void __init_text tss_init(void) {
	ptr_t stack;

	/* Set up the contents of the TSS. */
	memset(&curr_cpu->arch.tss, 0, sizeof(tss_t));
	curr_cpu->arch.tss.ss0 = SEGMENT_K_DS;
	curr_cpu->arch.tss.io_bitmap = 104;

	/* Set up the doublefault TSS. */
	memset(&curr_cpu->arch.double_fault_tss, 0, sizeof(tss_t));
	stack = (ptr_t)curr_cpu->arch.double_fault_stack;
	curr_cpu->arch.double_fault_tss.cr3 = x86_read_cr3();
	curr_cpu->arch.double_fault_tss.eip = (ptr_t)&isr_array[FAULT_DOUBLE];
	curr_cpu->arch.double_fault_tss.eflags = X86_FLAGS_ALWAYS1;
	curr_cpu->arch.double_fault_tss.esp = (stack + KSTACK_SIZE) - STACK_DELTA;
	curr_cpu->arch.double_fault_tss.es = SEGMENT_K_DS;
	curr_cpu->arch.double_fault_tss.cs = SEGMENT_K_CS;
	curr_cpu->arch.double_fault_tss.ss = SEGMENT_K_DS;
	curr_cpu->arch.double_fault_tss.ds = SEGMENT_K_DS;

	/* Load the TSS segment into TR. */
	ltr(SEGMENT_TSS);
}

/** Configure an IDT entry.
 * @param num		Entry number.
 * @param addr		Address of the handler.
 * @param seg		Code segment to execute handler in.
 * @param flags		Flags for the entry. */
static inline void idt_set_entry(int num, ptr_t addr, uint16_t seg, uint8_t flags) {
	kernel_idt[num].base0 = (addr & 0xFFFF);
	kernel_idt[num].base1 = ((addr >> 16) & 0xFFFF);
	kernel_idt[num].sel = seg;
	kernel_idt[num].unused = 0;
	kernel_idt[num].flags = flags;
}

/** Initialise the IDT shared by all CPUs. */
static inline void idt_init(void) {
	int i;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		idt_set_entry(i, (ptr_t)&isr_array[i], SEGMENT_K_CS, 0x8E);
	}

	/* Modify the double fault entry to become a task gate using the
	 * doublefault TSS. */
	idt_set_entry(FAULT_DOUBLE, 0, SEGMENT_DF_TSS, 0xE5);

	/* Set up the system call interrupt handler. It does not go through
	 * the usual route for interrupts because it doesn't need to do some
	 * things that are done there, and it also needs to do some special
	 * things. */
	idt_set_entry(SYSCALL_INT_NO, (ptr_t)syscall_entry, SEGMENT_K_CS, 0xEE);
}

/** Initialise descriptor tables for the boot CPU. */
void __init_text descriptor_init(void) {
	/* Cannot call tss_init() here as it relies on the kernel page map
	 * being set up to be able to set up the double fault TSS correctly.
	 * It will be called in arch_postmm_init(). */
	gdt_init();

	/* The IDT only needs to be initialised once. Do that now as we are on
	 * the boot CPU. */
	idt_init();

	/* Point the CPU to the new IDT. */
	lidt((ptr_t)&kernel_idt, (sizeof(kernel_idt) - 1));
}

/** Initialise descriptor tables for an application CPU. */
void __init_text descriptor_ap_init(void) {
	/* The GDT/TSS setup procedures are the same on both the BSP and APs,
	 * so just call the functions for them. The kernel page map will have
	 * been switched to when this is called so it is OK to call tss_init()
	 * here. */
	gdt_init();
	tss_init();

	/* For the IDT, there is no need to have a seperate IDT for each CPU,
	 * so just point the IDTR at the shared IDT. */
	lidt((ptr_t)&kernel_idt, (sizeof(kernel_idt) - 1));
}
