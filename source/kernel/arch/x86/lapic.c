/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		x86 local APIC code.
 */

#include <arch/io.h>

#include <x86/cpu.h>
#include <x86/lapic.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <kboot.h>
#include <kdbg.h>
#include <time.h>

#if CONFIG_SMP
KBOOT_BOOLEAN_OPTION("lapic_disabled", "Disable Local APIC usage (disables SMP)", false);
#else
KBOOT_BOOLEAN_OPTION("lapic_disabled", "Disable Local APIC usage", false);
#endif

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

#if CONFIG_SMP
extern void ipi_process_pending(void);
#endif

/** Local APIC mapping. If NULL the LAPIC is not present. */
static volatile uint32_t *lapic_mapping = NULL;

/** Local APIC base address. */
static phys_ptr_t lapic_base = 0;

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
 * @param frame		Interrupt stack frame. */
static void lapic_spurious_handler(unative_t num, intr_frame_t *frame) {
	kprintf(LOG_DEBUG, "lapic: received spurious interrupt\n");
}

#if CONFIG_SMP
/** IPI message interrupt handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame. */
static void lapic_ipi_handler(unative_t num, intr_frame_t *frame) {
	ipi_process_pending();
	lapic_eoi();
}
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
static void lapic_timer_prepare(useconds_t us) {
	uint32_t count = (uint32_t)((curr_cpu->arch.lapic_timer_cv * us) >> 32);
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
 * @param frame		Interrupt stack frame. */
static void lapic_timer_handler(unative_t num, intr_frame_t *frame) {
	curr_cpu->should_preempt = timer_tick();
	lapic_eoi();
}

/** Return whether the LAPIC enabled.
 * @return		Whether the LAPIC is enabled. */
bool lapic_enabled(void) {
	return lapic_mapping;
}

/** Get the current local APIC ID.
 * @return		Local APIC ID. */
uint32_t lapic_id(void) {
	if(!lapic_mapping) {
		return 0;
	}
	return (lapic_read(LAPIC_REG_APIC_ID) >> 24);
}

/** Send an IPI.
 * @param dest		Destination Shorthand.
 * @param id		Destination local APIC ID (if APIC_IPI_DEST_SINGLE).
 * @param mode		Delivery Mode.
 * @param vector	Value of vector field. */
void lapic_ipi(uint8_t dest, uint8_t id, uint8_t mode, uint8_t vector) {
	bool state;

	/* Must perform this check to prevent problems if fatal() is called
	 * before we've initialised the LAPIC. */
	if(!lapic_mapping) {
		return;
	}

	state = intr_disable();

	/* Write the destination ID to the high part of the ICR. */
	lapic_write(LAPIC_REG_ICR1, ((uint32_t)id << 24));

	/* Send the IPI:
	 * - Destination Mode: Physical.
	 * - Level: Assert (bit 14).
	 * - Trigger Mode: Edge. */
	lapic_write(LAPIC_REG_ICR0, (1<<14) | (dest << 18) | (mode << 8) | vector);

	/* Wait for the IPI to be sent (check Delivery Status bit). */
	while(lapic_read(LAPIC_REG_ICR0) & (1<<12)) {
		__asm__ volatile("pause");
	}

	intr_restore(state);
}

#if CONFIG_SMP
/** Send an IPI interrupt to a single CPU.
 * @param dest		Destination CPU ID. */
void ipi_arch_interrupt(cpu_id_t dest) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)dest, LAPIC_IPI_FIXED, LAPIC_VECT_IPI);
}
#endif

/** Function to calculate the LAPIC timer frequency.
 * @return		Calculated frequency. */
static __init_text uint64_t calculate_lapic_frequency(void) {
	uint16_t shi, slo, ehi, elo, pticks;
	uint64_t end, lticks;

	/* First set the PIT to rate generator mode. */
	out8(0x43, 0x34);
	out8(0x40, 0xFF);
	out8(0x40, 0xFF);

	/* Wait for the cycle to begin. */
	do {
		out8(0x43, 0x00);
		slo = in8(0x40);
		shi = in8(0x40);
	} while(shi != 0xFF);

	/* Kick off the LAPIC timer. */
	lapic_write(LAPIC_REG_TIMER_INITIAL, 0xFFFFFFFF);

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the current timer value. */
	end = lapic_read(LAPIC_REG_TIMER_CURRENT);

	/* Calculate the differences between the values. */
	lticks = 0xFFFFFFFF - end;
	pticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (lticks * 8 * PIT_FREQUENCY) / pticks;
}

/** Initialise the local APIC on the current CPU. */
__init_text void lapic_init(void) {
	uint64_t base;

	/* Don't do anything if we don't have LAPIC support or have been asked
	 * not to use the LAPIC. */
	if(!cpu_features.apic || kboot_boolean_option("lapic_disabled")) {
		return;
	}

	/* Get the base address of the LAPIC mapping. If bit 11 is 0, the LAPIC
	 * is disabled. */
	base = x86_read_msr(X86_MSR_APIC_BASE);
	if(!(base & (1<<11))) {
		return;
	} else if(cpu_features.x2apic && base & (1<<10)) {
		fatal("Cannot handle CPU %u in x2APIC mode", curr_cpu->id);
	}
	base &= 0xFFFFF000;

#if CONFIG_SMP
	if(lapic_mapping) {
		/* This is a secondary CPU. Ensure that the base address is
		 * not different to the boot CPU's. */
		if(base != lapic_base) {
			fatal("CPU %u has different LAPIC address to boot CPU", curr_cpu->id);
		}
	} else {
#endif
		/* This is the boot CPU. Map the LAPIC into virtual memory and
		 * register interrupt vector handlers. */
		lapic_base = base;
		lapic_mapping = phys_map(base, PAGE_SIZE, MM_FATAL);
		kprintf(LOG_NORMAL, "lapic: physical location 0x%" PRIxPHYS ", mapped to %p\n",
		        base, lapic_mapping);

		intr_register(LAPIC_VECT_SPURIOUS, lapic_spurious_handler);
		intr_register(LAPIC_VECT_TIMER, lapic_timer_handler);
#if CONFIG_SMP
		intr_register(LAPIC_VECT_IPI, lapic_ipi_handler);
	}
#endif

	/* Enable the local APIC (bit 8) and set the spurious interrupt
	 * vector in the Spurious Interrupt Vector Register. */
	lapic_write(LAPIC_REG_SPURIOUS, LAPIC_VECT_SPURIOUS | (1<<8));
	lapic_write(LAPIC_REG_TIMER_DIVIDER, LAPIC_TIMER_DIV8);

	/* Calculate LAPIC frequency. See comment about CPU frequency in QEMU
	 * in cpu_arch_init(), same applies here. */
#if CONFIG_SMP
	if(strncmp(curr_cpu->arch.model_name, "QEMU", 4) != 0 || curr_cpu == &boot_cpu) {
#endif
		curr_cpu->arch.lapic_freq = calculate_frequency(calculate_lapic_frequency);
#if CONFIG_SMP
	} else {
		curr_cpu->arch.lapic_freq = boot_cpu.arch.lapic_freq;
	}
#endif

	/* Figure out the timer conversion factor. */
	curr_cpu->arch.lapic_timer_cv = ((curr_cpu->arch.lapic_freq / 8) << 32) / 1000000;
	kprintf(LOG_NORMAL, "lapic: timer conversion factor for CPU %u is %u (freq: %" PRIu64 "MHz)\n",
	        curr_cpu->id, curr_cpu->arch.lapic_timer_cv,
	        curr_cpu->arch.lapic_freq / 1000000);

	/* Accept all interrupts. */
	lapic_write(LAPIC_REG_TPR, lapic_read(LAPIC_REG_TPR & 0xFFFFFF00));

	/* Set the timer device. */
	timer_device_set(&lapic_timer_device);
}
