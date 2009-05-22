/* Kiwi x86 local APIC code
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
 * @brief		x86 local APIC code.
 */

#include <arch/asm.h>
#include <arch/io.h>
#include <arch/x86/features.h>
#include <arch/x86/lapic.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/irq.h>

#include <mm/page.h>
#include <mm/tlb.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern bool cpu_ipi_schedule_handler(unative_t num, intr_frame_t *frame);

/** Whether the local APIC is present and enabled. */
bool lapic_enabled = false;

/** Local APIC mapping on the kernel heap. */
static volatile uint32_t *lapic_mapping = NULL;

/** Read from a register in the current CPU's local APIC.
 * @param reg		Register to read from.
 * @return		Value read from register. */
static inline uint32_t lapic_read(int reg) {
	return lapic_mapping[reg];
}

/** Write to a register in the current CPU's local APIC.
 * @param reg		Register to write to.
 * @param value		Value to write to register. */
static inline void lapic_write(int reg, uint32_t value) {
	lapic_mapping[reg] = value;
}

/** Send an EOI to the local APIC. */
static inline void lapic_eoi(void) {
	lapic_write(LAPIC_REG_EOI, 0);
}

/** Spurious interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
static bool lapic_spurious_handler(unative_t num, intr_frame_t *frame) {
	kprintf(LOG_DEBUG, "lapic: received spurious interrupt\n");
	return false;
}

#if CONFIG_SMP
/** Reschedule interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		True if current thread should be preempted. */
static bool lapic_schedule_handler(unative_t num, intr_frame_t *frame) {
	lapic_eoi();
	return cpu_ipi_schedule_handler(num, frame);
}

/** TLB shootdown interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		True if current thread should be preempted. */
static bool lapic_tlb_shootdown_handler(unative_t num, intr_frame_t *frame) {
	bool ret = tlb_shootdown_responder(num, frame);
	lapic_eoi();
	return ret;
}
#endif

/*
 * Local APIC timer functions.
 */

/** Prepare local APIC timer tick.
 * @param ns		Number of nanoseconds to tick in. */
static void lapic_timer_prep(uint64_t ns) {
	uint32_t count = (uint32_t)((curr_cpu->arch.lapic_freq * ns) >> 32);
	lapic_write(LAPIC_REG_TIMER_INITIAL, (count == 0 && ns != 0) ? 1 : count);
}

/** Enable the local APIC timer. */
static void lapic_timer_enable(void) {
	/* Set the interrupt vector, no extra bits = Unmasked/One-shot. */
	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_VECT_TIMER);
}

/** Disable the local APIC timer. */
static void lapic_timer_disable(void) {
	/* Set bit 16 in the Timer LVT register to 1 (Masked) */
	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_VECT_TIMER | (1<<16));
}

/** Local APIC clock source. */
static clock_source_t lapic_clock_source = {
	.name = "LAPIC",
	.type = CLOCK_ONESHOT,

	.prep = lapic_timer_prep,
	.enable = lapic_timer_enable,
	.disable = lapic_timer_disable,
};

/** Timer interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Value from clock_tick(). */
static bool lapic_timer_handler(unative_t num, intr_frame_t *frame) {
	bool ret = clock_tick();

	lapic_eoi();
	return ret;
}

/*
 * Main functions.
 */

/** Tick count used during CPU bus frequency calculation. */
static volatile uint32_t freq_tick_count = 0;

/** PIT handler for bus frequency calculation. */
static bool lapic_pit_handler(unative_t irq, intr_frame_t *frame) {
	freq_tick_count++;
	return false;
}

/** Find out the CPU bus frequency.
 * @todo		This shouldn't depend on platform-specific stuff.
 * @return		CPU bus frequency. */
static uint64_t lapic_get_freq(void) {
	uint64_t current;
	uint16_t base;
	uint32_t old;

	assert(intr_state() == false);

	/* Set the PIT at 50Hz. */
	base = 1193182L / 50;
	out8(0x43, 0x36);
	out8(0x40, base & 0xFF);
	out8(0x40, base >> 8);

	/* Set our temporary PIT handler. */
	if(irq_register(0, lapic_pit_handler) != 0 || irq_unmask(0)) {
		fatal("APIC could not grab PIT");
	}

	/* Enable interrupts and wait for the start of the next timer tick */
	old = freq_tick_count;
	intr_enable();
	while(freq_tick_count == old);

	/* Enable the APIC timer. */
	lapic_timer_enable();
	lapic_write(LAPIC_REG_TIMER_INITIAL, 0xFFFFFFFF);

	/* Wait for the next tick to occur. */
	old = freq_tick_count;
	while(freq_tick_count == old);

	/* Stop the APIC timer and get the current count. */
	lapic_timer_disable();
	current = (uint64_t)lapic_read(LAPIC_REG_TIMER_CURRENT);

	/* Stop the PIT. */
	intr_disable();
	assert(irq_remove(0) == 0);

	/* Frequency is the difference between initial and current multiplied
	 * by the PIT frequency. */
	return (0xFFFFFFFF - current) * 8 * 50;
}

/** Get the current local APIC ID.
 *
 * Gets the local APIC ID of the current CPU.
 *
 * @return		Local APIC ID.
 */
uint32_t lapic_id(void) {
	if(!lapic_enabled || !lapic_mapping) {
		return 0;
	}
	return (lapic_read(LAPIC_REG_APIC_ID) >> 24);
}

/** Send an IPI.
 *
 * Sends an inter-processor interrupt (IPI).
 *
 * @param dest		Destination Shorthand.
 * @param id		Destination local APIC ID (if APIC_IPI_DEST_SINGLE).
 * @param mode		Delivery Mode.
 * @param vector	Value of vector field.
 */
void lapic_ipi(uint8_t dest, uint8_t id, uint8_t mode, uint8_t vector) {
	/* Must perform this check to prevent problems if fatal() is called
	 * before we've initialized the LAPIC. */
	if(!lapic_enabled || !lapic_mapping) {
		return;
	}

	/* Write the destination ID to the high part of the ICR. */
	lapic_write(LAPIC_REG_ICR1, ((uint32_t)id << 24));

	/* Send the IPI:
	 * - Destination Mode: Physical.
	 * - Level: Assert (bit 14).
	 * - Trigger Mode: Edge. */
	lapic_write(LAPIC_REG_ICR0, (1<<14) | (dest << 18) | (mode << 8) | vector);
}

/** Initialize the local APIC.
 *
 * Maps the local APIC if it has not already been mapped and initializes the
 * current CPU's local APIC.
 *
 * @todo		If APIC is disabled in MSR, enable it if the APIC is
 *			not based on the APIC bus.
 *
 * @return		Whether initialization succeeded.
 */
bool lapic_init(void) {
	phys_ptr_t base;

	if(!CPU_HAS_APIC(curr_cpu)) {
		return false;
	}

	base = sysreg_msr_read(SYSREG_MSR_APIC_BASE);

	/* If bit 11 is 0, the APIC is disabled (see above todo). */
	if(!(base & (1<<11))) {
		return false;
	}

	/* If the mapping is not set, we're being run on the BSP. Create it,
	 * set the clock source, and register interrupt vector handlers. */
	if(!lapic_mapping) {
		/* Map on the kernel heap. */
		lapic_mapping = page_phys_map(base & PAGE_MASK, PAGE_SIZE, MM_FATAL);

		/* Grab interrupt vectors. */
		intr_register(LAPIC_VECT_SPURIOUS, lapic_spurious_handler);
		intr_register(LAPIC_VECT_TIMER, lapic_timer_handler);
#if CONFIG_SMP
		intr_register(IPI_SCHEDULE, lapic_schedule_handler);
		intr_register(IPI_TLB_SHOOTDOWN, lapic_tlb_shootdown_handler);
#endif
	}

	/* Enable the local APIC (bit 8) and set the spurious interrupt
	 * vector in the Spurious Interrupt Vector Register. */
	lapic_write(LAPIC_REG_SPURIOUS, LAPIC_VECT_SPURIOUS | (1<<8));
	lapic_write(LAPIC_REG_TIMER_DIVIDER, LAPIC_TIMER_DIV8);

	/* Figure out the CPU bus frequency. */
	curr_cpu->arch.lapic_freq = ((lapic_get_freq() / 8) << 32) / 1000000000;

	/* Set the clock source. */
	if(clock_source_set(&lapic_clock_source) != 0) {
		fatal("Could not set LAPIC clock source");
	}

	lapic_enabled = true;
	return true;
}
