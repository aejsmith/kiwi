/* Kiwi x86 low-level interrupt functions
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
 * @brief		Low-level interrupt functions.
 */

#include <arch/asm.h>
#include <arch/defs.h>
#include <arch/gdt.h>
#include <arch/intr.h>
#include <arch/io.h>
#include <arch/mem.h>

#include <console/kprintf.h>

#include <cpu/intr.h>
#include <cpu/irq.h>

#include <fatal.h>
#include <kdbg.h>

extern bool kdbg_int1_handler(unative_t num, intr_frame_t *regs);
extern bool pagefault_handler(unative_t num, intr_frame_t *regs);

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t __isr_array[INTR_COUNT][16];
extern atomic_t fatal_protect;

/** Array of IDT entries. */
static idt_entry_t idt[INTR_COUNT] __aligned(8);

/** String names for CPU exceptions. */
static const char *intr_except_strs[] = {
	"Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
	"INTO Detected Overflow", "Out of Bounds", "Invalid Opcode",
	"No Coprocessor", "Double Fault", "Coprocessor Segment Overrun",
	"Bad TSS", "Segment Not Present", "Stack Fault",
	"General Protection Fault", "Page Fault", "Unknown Interrupt",
	"Coprocessor Fault", "Alignment Check", "Machine Check",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved"
};

/** Handler for exceptions.
 * @param num		CPU interrupt number.
 * @param regs		Register dump.
 * @return		True if preemption required, false if not. */
static bool intr_except_handler(unative_t num, intr_frame_t *regs) {
	if(atomic_get(&kdbg_running) == 2) {
		kdbg_except_handler(num, intr_except_strs[num], regs);
		return false;
	} else if(num == 2) {
		/* Non-Maskable Interrupt. If currently in fatal(), assume that
		 * we're being asked to halt by the CPU that called fatal(). */
		if(atomic_get(&fatal_protect) != 0) {
			intr_disable();
			while(true) {
				idle();
			}
		} else if(atomic_get(&kdbg_running)) {
			while(atomic_get(&kdbg_running));
			return false;
		}
	}

	_fatal(regs, "Unhandled kernel exception %" PRIun " (%s)", num, intr_except_strs[num]);
}

/** Handler for double faults.
 * @param num		CPU interrupt number.
 * @param regs		Register dump.
 * @return		Doesn't return. */
static bool intr_doublefault_handler(unative_t num, intr_frame_t *regs) {
	/* Disable KDBG. */
	atomic_set(&kdbg_running, 3);

	_fatal(regs, "Double Fault (0x%p)", regs->ip);
	while(true) {
		idle();
	}
}

#if CONFIG_ARCH_64BIT
/** Set up an IDT entry.
 * @param num		IDT entry number.
 * @param addr		Address of ISR. */
static void intr_idt_entry_init(unative_t num, ptr_t addr) {
	idt[num].base0 = (addr & 0xFFFF);
	idt[num].base1 = ((addr >> 16) & 0xFFFF);
	idt[num].base2 = ((addr >> 32) & 0xFFFFFFFF);
	idt[num].ist = 0;
	idt[num].reserved = 0;
	idt[num].sel = SEG_K_CS;
	idt[num].unused = 0;
	idt[num].flags = 0x8E;
}

/** Set up the double fault handler. */
static void intr_mach_init(void) {
	/* The GDT code sets up the first IST entry to point to the
	 * double fault stack. */
	idt[8].ist = 1;
}
#else
/** Set up an IDT entry.
 * @param num		IDT entry number.
 * @param addr		Address of ISR. */
static void intr_idt_entry_init(unative_t num, ptr_t addr) {
	idt[num].base0 = (addr & 0xFFFF);
	idt[num].base1 = ((addr >> 16) & 0xFFFF);
	idt[num].sel = SEG_K_CS;
	idt[num].unused = 0;
	idt[num].flags = 0x8E;
}

/** Set up the double fault TSS. */
static void intr_mach_init(void) {
	__doublefault_tss.cr3 = read_cr3();
	__doublefault_tss.eip = (ptr_t)&__isr_array[8];
	__doublefault_tss.eflags = X86_FLAGS_ALWAYS1;
	__doublefault_tss.esp = ((ptr_t)__doublefault_stack + KSTACK_SIZE) - STACK_DELTA;
	__doublefault_tss.es = SEG_K_DS;
	__doublefault_tss.cs = SEG_K_CS;
	__doublefault_tss.ss = SEG_K_DS;
	__doublefault_tss.ds = SEG_K_DS;

	idt[8].flags = 0x85;
	idt[8].sel = SEG_DF_TSS;
	idt[8].base0 = 0;
	idt[8].base1 = 0;
}
#endif

/** Initialize the IDT/interrupt handling. */
void intr_init(void) {
	unative_t i;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < INTR_COUNT; i++) {
		intr_idt_entry_init(i, (ptr_t)&__isr_array[i]);
	}

	/* Point the processor to the new IDT. */
	lidt((ptr_t)&idt, (sizeof(idt) - 1));

	/* Now we can fill out the interrupt handler table. Entries 0-31 are
	 * exceptions. */
	for(i = 0; i <= 31; i++) {
		intr_register(i, intr_except_handler);
	}

	/* Entries 32-47 are IRQs, 48 onwards are unrecognised for now (later
	 * on the system call interrupt will be added). */
	for(i = 32; i <= 47; i++) {
		intr_register(i, irq_handler);
	}

	/* Add debug exception for KDBG. */
	intr_register(1, kdbg_int1_handler);

	/* Add double fault handler. */
	intr_register(8, intr_doublefault_handler);

	/* Add page fault handler. */
	intr_register(14, pagefault_handler);

	/* Machine-specific initialization. */
	intr_mach_init();
}

/** Load the IDTR on an AP. */
void intr_ap_init(void) {
	lidt((ptr_t)&idt, (sizeof(idt) - 1));
}
