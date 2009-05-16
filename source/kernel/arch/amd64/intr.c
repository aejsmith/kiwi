/* Kiwi AMD64 interrupt setup code.
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
 * @brief		AMD64 interrupt setup code.
 */

#include <arch/asm.h>
#include <arch/fault.h>
#include <arch/gdt.h>
#include <arch/intr.h>

#include <cpu/intr.h>
#include <cpu/irq.h>

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t __isr_array[INTR_COUNT][16];

/** Array of IDT entries. */
static idt_entry_t idt[INTR_COUNT] __aligned(8);

/** Initialize the IDT/interrupt handling. */
void intr_init(void) {
	unative_t i;
	ptr_t addr;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < INTR_COUNT; i++) {
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

	/* The GDT code sets up the first IST entry to point to the double
	 * fault stack. Point the double fault IDT entry at this stack. */
	idt[FAULT_DOUBLE].ist = 1;

	/* Point the processor to the new IDT. */
	lidt((ptr_t)&idt, (sizeof(idt) - 1));

	/* Now we can fill out the interrupt handler table. Entries 0-31 are
	 * exceptions. */
	for(i = 0; i < FAULT_COUNT; i++) {
		intr_register(i, fault_handler);
	}

	/* Entries 32-47 are IRQs, 48 onwards are unrecognised for now (later
	 * on the system call interrupt will be added). */
	for(i = 32; i <= 47; i++) {
		intr_register(i, irq_handler);
	}
}

/** Load the IDTR on an AP. */
void intr_ap_init(void) {
	lidt((ptr_t)&idt, (sizeof(idt) - 1));
}
