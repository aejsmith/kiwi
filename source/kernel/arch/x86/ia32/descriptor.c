/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		IA32 descriptor table functions.
 */

#include <arch/memory.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

extern void syscall_entry(void);

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Array of GDT descriptors. */
static gdt_entry_t initial_gdt[GDT_ENTRY_COUNT] __aligned(8) = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },		/**< NULL descriptor. */
	{ 0xFFFF, 0, 0, 0x9A, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel CS (Code). */
	{ 0xFFFF, 0, 0, 0x92, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel DS (Data). */
	{ 0xFFFF, 0, 0, 0x92, 0xF, 0, 0, 1, 1, 0 },	/**< Kernel GS (CPU pointer). */
	{ 0xFFFF, 0, 0, 0xFE, 0xF, 0, 0, 1, 1, 0 },	/**< User CS (Code). */
	{ 0xFFFF, 0, 0, 0xF2, 0xF, 0, 0, 1, 1, 0 },	/**< User DS (Data). */
	{ 0xFFFF, 0, 0, 0xF2, 0xF, 0, 0, 1, 1, 0 },	/**< User GS (TLS). */
	{ 0, 0, 0, 0x89, 0, 0, 0, 1, 0, 0 },		/**< TSS descriptor. */
	{ 0, 0, 0, 0x89, 0, 0, 0, 1, 0, 0 },		/**< Doublefault TSS descriptor. */
};

/** Array of IDT entries. */
static idt_entry_t kernel_idt[IDT_ENTRY_COUNT] __aligned(8);

/** Set the base address of a segment.
 * @param cpu		CPU to modify GDT for.
 * @param sel		Segment to modify.
 * @param base		New base address of the segment. */
void gdt_set_base(cpu_t *cpu, int sel, ptr_t base) {
	cpu->arch.gdt[sel / 0x08].base0 = (base & 0xFFFF);
	cpu->arch.gdt[sel / 0x08].base1 = (base >> 16) & 0xFF;
	cpu->arch.gdt[sel / 0x08].base2 = (base >> 24) & 0xFF;
}

/** Set the limit of a segment.
 * @param cpu		CPU to modify GDT for.
 * @param sel		Segment to modify.
 * @param limit		New limit of the segment. */
void gdt_set_limit(cpu_t *cpu, int sel, size_t limit) {
	cpu->arch.gdt[sel / 0x08].limit0 = (limit & 0xFFFF);
	cpu->arch.gdt[sel / 0x08].limit1 = (limit >> 16) & 0xF;
}

/** Set up the GDT for the current CPU.
 * @param cpu		CPU to initialise for. */
static __init_text void gdt_init(cpu_t *cpu) {
	/* Create a copy of the statically allocated GDT. */
	memcpy(cpu->arch.gdt, initial_gdt, sizeof(initial_gdt));

	/* Set up the TSS descriptor. */
	gdt_set_base(cpu, SEGMENT_TSS, (ptr_t)&cpu->arch.tss);
	gdt_set_limit(cpu, SEGMENT_TSS, sizeof(tss_t));
	gdt_set_base(cpu, SEGMENT_DF_TSS, (ptr_t)&cpu->arch.double_fault_tss);
	gdt_set_limit(cpu, SEGMENT_DF_TSS, sizeof(tss_t));

	/* Although once the thread system is up the GS base is pointed at the
	 * architecture thread data, we need curr_cpu to work before that. Our
	 * CPU data has a pointer at the start which we can use, so point the
	 * GS base at that to begin with. */
	gdt_set_base(cpu, SEGMENT_K_GS, (ptr_t)&cpu->arch);

	/* Set the GDT pointer. */
	lgdt((ptr_t)&cpu->arch.gdt, sizeof(cpu->arch.gdt) - 1);

	/* Reload the segment registers. */
	__asm__ volatile(
		"ljmp	%0, $1f\n"
		"1:\n"
		"mov	%1, %%ds\n"
		"mov	%1, %%es\n"
		"mov	%1, %%fs\n"
		"mov	%1, %%ss\n"
		"mov	%2, %%gs\n"
		:: "i"(SEGMENT_K_CS), "r"(SEGMENT_K_DS), "r"(SEGMENT_K_GS)
	);
}

/** Set up the TSS for the current CPU.
 * @param cpu		CPU to initialise for. */
static __init_text void tss_init(cpu_t *cpu) {
	ptr_t stack;

	/* Set up the contents of the TSS. */
	memset(&cpu->arch.tss, 0, sizeof(tss_t));
	cpu->arch.tss.ss0 = SEGMENT_K_DS;
	cpu->arch.tss.io_bitmap = 104;

	/* Set up the doublefault TSS. Note that when we're executed on the
	 * boot CPU, we'll be on the bootloader's CR3. The CR3 field is updated
	 * to the kernel PDP later on by arch_postmm_init(). */
	memset(&cpu->arch.double_fault_tss, 0, sizeof(tss_t));
	stack = (ptr_t)cpu->arch.double_fault_stack;
	cpu->arch.double_fault_tss.cr3 = x86_read_cr3();
	cpu->arch.double_fault_tss.eip = (ptr_t)&isr_array[X86_EXCEPT_DF];
	cpu->arch.double_fault_tss.eflags = X86_FLAGS_ALWAYS1;
	cpu->arch.double_fault_tss.esp = stack + KSTACK_SIZE;
	cpu->arch.double_fault_tss.es = SEGMENT_K_DS;
	cpu->arch.double_fault_tss.cs = SEGMENT_K_CS;
	cpu->arch.double_fault_tss.ss = SEGMENT_K_DS;
	cpu->arch.double_fault_tss.ds = SEGMENT_K_DS;
	cpu->arch.double_fault_tss.gs = SEGMENT_K_GS;

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
static __init_text void idt_init(void) {
	int i;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		idt_set_entry(i, (ptr_t)&isr_array[i], SEGMENT_K_CS, 0x8E);
	}

	/* Modify the double fault entry to become a task gate using the
	 * doublefault TSS. */
	idt_set_entry(X86_EXCEPT_DF, 0, SEGMENT_DF_TSS, 0xE5);

	/* Set up the system call interrupt handler. It does not go through
	 * the usual route for interrupts because it doesn't need to do some
	 * things that are done there, and it also needs to do some special
	 * things. */
	idt_set_entry(SYSCALL_INT_NO, (ptr_t)syscall_entry, SEGMENT_K_CS, 0xEE);
}

/** Initialise descriptor tables for the current CPU.
 * @param cpu		CPU to initialise for. */
__init_text void descriptor_init(cpu_t *cpu) {
	gdt_init(cpu);
	tss_init(cpu);

	/* The IDT only needs to be initialised once on the boot CPU. */
	if(cpu == &boot_cpu) {
		idt_init();
	}

	/* Point the CPU to the new IDT. */
	lidt((ptr_t)&kernel_idt, (sizeof(kernel_idt) - 1));
}
