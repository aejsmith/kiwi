/*
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

#include <arch/features.h>
#include <arch/io.h>
#include <arch/lapic.h>
#include <arch/sysreg.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <mm/page.h>
#include <mm/tlb.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern void ipi_process_pending(void);

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

/** IPI message interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
static bool lapic_ipi_handler(unative_t num, intr_frame_t *frame) {
	ipi_process_pending();
	lapic_eoi();
	return false;
}

/** Reschedule IPI interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns true. */
static bool lapic_reschedule_handler(unative_t num, intr_frame_t *frame) {
	lapic_eoi();
	return true;
}

#if 0
# pragma mark Local APIC timer functions.
#endif

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

/** Prepare local APIC timer tick.
 * @param us		Number of microseconds to tick in. */
static void lapic_timer_prepare(timeout_t us) {
	uint32_t count = (uint32_t)((curr_cpu->arch.lapic_freq * us) >> 32);
	lapic_write(LAPIC_REG_TIMER_INITIAL, (count == 0 && us != 0) ? 1 : count);
}

/** Local APIC timer device. */
static timer_device_t lapic_timer_device = {
	.name = "LAPIC",
	.type = TIMER_DEVICE_ONESHOT,
	.enable = lapic_timer_enable,
	.disable = lapic_timer_disable,
	.prepare = lapic_timer_prepare,
};

/** Timer interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Return value from clock_tick(). */
static bool lapic_timer_handler(unative_t num, intr_frame_t *frame) {
	bool ret = timer_tick();
	lapic_eoi();
	return ret;
}

#if 0
# pragma mark -
#endif

/** Tick count used during CPU bus frequency calculation. */
static volatile uint32_t freq_tick_count __init_data = 0;

/** PIT handler for bus frequency calculation. */
static irq_result_t __init_text lapic_pit_handler(unative_t irq, void *data, intr_frame_t *frame) {
	freq_tick_count++;
	return IRQ_HANDLED;
}

/** Find out the CPU bus frequency.
 * @todo		This shouldn't depend on platform-specific stuff.
 * @return		CPU bus frequency. */
static uint64_t __init_text lapic_get_freq(void) {
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
	if(irq_register(0, lapic_pit_handler, NULL, NULL) != 0) {
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
	if(irq_unregister(0, lapic_pit_handler, NULL, NULL) != 0) {
		fatal("Could not remove PIT IRQ handler");
	}

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
	 * before we've initialised the LAPIC. */
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

/** Initialise the local APIC.
 *
 * Maps the local APIC if it has not already been mapped and initialises the
 * current CPU's local APIC.
 *
 * @todo		If APIC is disabled in MSR, enable it if the APIC is
 *			not based on the APIC bus.
 *
 * @return		Whether initialisation succeeded.
 */
bool __init_text lapic_init(void) {
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
		intr_register(LAPIC_VECT_IPI, lapic_ipi_handler);
		intr_register(LAPIC_VECT_RESCHEDULE, lapic_reschedule_handler);
	}

	/* Enable the local APIC (bit 8) and set the spurious interrupt
	 * vector in the Spurious Interrupt Vector Register. */
	lapic_write(LAPIC_REG_SPURIOUS, LAPIC_VECT_SPURIOUS | (1<<8));
	lapic_write(LAPIC_REG_TIMER_DIVIDER, LAPIC_TIMER_DIV8);

	/* Figure out the CPU bus frequency. */
	curr_cpu->arch.lapic_freq = ((lapic_get_freq() / 8) << 32) / 1000000;

	/* Set the timer device. */
	timer_device_set(&lapic_timer_device);

	lapic_enabled = true;
	return true;
}
