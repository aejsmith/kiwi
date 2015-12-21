/*
 * Copyright (C) 2008-2014 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               AMD64 local APIC code.
 */

#include <arch/io.h>

#include <x86/cpu.h>
#include <x86/interrupt.h>
#include <x86/lapic.h>

#include <lib/string.h>

#include <mm/phys.h>

#include <pc/pit.h>

#include <cpu.h>
#include <kboot.h>
#include <kernel.h>
#include <smp.h>
#include <time.h>

KBOOT_BOOLEAN_OPTION("lapic_disabled", "Disable Local APIC usage (disables SMP)", false);

/** Local APIC mapping. If NULL the LAPIC is not present. */
static volatile uint32_t *lapic_mapping;

/** Local APIC base address. */
static phys_ptr_t lapic_base;

/** Read from a register in the current CPU's local APIC.
 * @param reg           Register to read from.
 * @return              Value read from register. */
static inline uint32_t lapic_read(unsigned reg) {
    return lapic_mapping[reg];
}

/** Write to a register in the current CPU's local APIC.
 * @param reg           Register to write to.
 * @param value         Value to write to register. */
static inline void lapic_write(unsigned reg, uint32_t value) {
    lapic_mapping[reg] = value;
}

/** Send an EOI to the local APIC. */
static inline void lapic_eoi(void) {
    lapic_write(LAPIC_REG_EOI, 0);
}

/** Spurious interrupt handler.
 * @param frame         Interrupt stack frame. */
static void lapic_spurious_interrupt(frame_t *frame) {
    kprintf(LOG_DEBUG, "lapic: received spurious interrupt\n");
}

/** IPI interrupt handler.
 * @param frame         Interrupt stack frame. */
static void lapic_ipi_interrupt(frame_t *frame) {
    smp_ipi_handler();
    lapic_eoi();
}

/** Prepare local APIC timer tick.
 * @param nsecs         Number of nanoseconds to tick in. */
static void lapic_timer_prepare(nstime_t nsecs) {
    uint32_t count = (curr_cpu->arch.lapic_timer_cv * nsecs) >> 32;
    lapic_write(LAPIC_REG_TIMER_INITIAL, (count == 0 && nsecs != 0) ? 1 : count);
}

/** Local APIC timer device. */
static timer_device_t lapic_timer_device = {
    .name = "LAPIC",
    .type = TIMER_DEVICE_ONESHOT,
    .prepare = lapic_timer_prepare,
};

/** Timer interrupt handler.
 * @param frame         Interrupt stack frame. */
static void lapic_timer_interrupt(frame_t *frame) {
    curr_cpu->should_preempt = timer_tick();
    lapic_eoi();
}

/** Return whether the LAPIC enabled.
 * @return              Whether the LAPIC is enabled. */
bool lapic_enabled(void) {
    return lapic_mapping;
}

/** Get the current local APIC ID.
 * @return              Local APIC ID. */
uint32_t lapic_id(void) {
    return (lapic_mapping) ? (lapic_read(LAPIC_REG_APIC_ID) >> 24) : 0;
}

/** Send an IPI.
 * @param dest          Destination Shorthand.
 * @param id            Destination local APIC ID (if APIC_IPI_DEST_SINGLE).
 * @param mode          Delivery Mode.
 * @param vector        Value of vector field. */
void lapic_ipi(uint8_t dest, uint8_t id, uint8_t mode, uint8_t vector) {
    bool state;

    /* Must perform this check to prevent problems if fatal() is called before
     * we've initialized the LAPIC. */
    if (!lapic_mapping)
        return;

    state = local_irq_disable();

    /* Write the destination ID to the high part of the ICR. */
    lapic_write(LAPIC_REG_ICR1, ((uint32_t)id << 24));

    /* Send the IPI:
     * - Destination Mode: Physical.
     * - Level: Assert (bit 14).
     * - Trigger Mode: Edge. */
    lapic_write(
        LAPIC_REG_ICR0,
        (1 << 14) | ((uint32_t)dest << 18) | ((uint32_t)mode << 8) | (uint32_t)vector);

    /* Wait for the IPI to be sent (check Delivery Status bit). */
    while (lapic_read(LAPIC_REG_ICR0) & (1 << 12))
        arch_cpu_spin_hint();

    local_irq_restore(state);
}

/** Function to calculate the LAPIC timer frequency.
 * @return              Calculated frequency. */
static __init_text uint64_t calculate_lapic_frequency(void) {
    uint16_t shi, slo, ehi, elo, pticks;
    uint64_t end, lticks;

    /* First set the PIT to rate generator mode. */
    out8(0x43, 0x34);
    out8(0x40, 0xff);
    out8(0x40, 0xff);

    /* Wait for the cycle to begin. */
    do {
        out8(0x43, 0x00);
        slo = in8(0x40);
        shi = in8(0x40);
    } while (shi != 0xff);

    /* Kick off the LAPIC timer. */
    lapic_write(LAPIC_REG_TIMER_INITIAL, 0xffffffff);

    /* Wait for the high byte to drop to 128. */
    do {
        out8(0x43, 0x00);
        elo = in8(0x40);
        ehi = in8(0x40);
    } while (ehi > 0x80);

    /* Get the current timer value. */
    end = lapic_read(LAPIC_REG_TIMER_CURRENT);

    /* Calculate the differences between the values. */
    lticks = 0xffffffff - end;
    pticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

    /* Calculate frequency. */
    return (lticks * 8 * PIT_BASE_FREQUENCY) / pticks;
}

/** Initialize the local APIC. */
__init_text void lapic_init(void) {
    uint64_t base;

    /* Don't do anything if we don't have LAPIC support or have been asked not
     * to use the LAPIC. */
    if (!cpu_features.apic || kboot_boolean_option("lapic_disabled"))
        return;

    /* Get the base address of the LAPIC mapping. If bit 11 is 0, the LAPIC is
     * disabled. */
    base = x86_read_msr(X86_MSR_APIC_BASE);
    if (!(base & (1 << 11))) {
        return;
    } else if (cpu_features.x2apic && base & (1 << 10)) {
        fatal("Cannot handle LAPIC in x2APIC mode");
    }

    base &= 0xfffff000;

    /* Map the LAPIC into virtual memory and register interrupt handlers. */
    lapic_base = base;
    lapic_mapping = phys_map(base, PAGE_SIZE, MM_BOOT);
    kprintf(
        LOG_NOTICE, "lapic: physical location 0x%" PRIxPHYS ", mapped to %p\n",
        base, lapic_mapping);

    /* Install the LAPIC timer device. */
    timer_device_set(&lapic_timer_device);

    /* Install interrupt vectors. */
    interrupt_table[LAPIC_VECT_SPURIOUS] = lapic_spurious_interrupt;
    interrupt_table[LAPIC_VECT_TIMER] = lapic_timer_interrupt;
    interrupt_table[LAPIC_VECT_IPI] = lapic_ipi_interrupt;
}

/** Initialize the local APIC on the current CPU. */
__init_text void lapic_init_percpu(void) {
    if (!lapic_mapping)
        return;

    /* Enable the local APIC (bit 8) and set the spurious interrupt vector in
     * the Spurious Interrupt Vector Register. */
    lapic_write(LAPIC_REG_SPURIOUS, LAPIC_VECT_SPURIOUS | (1 << 8));
    lapic_write(LAPIC_REG_TIMER_DIVIDER, LAPIC_TIMER_DIV8);

    /* Calculate LAPIC frequency. See comment about CPU frequency in QEMU in
     * arch_cpu_early_init_percpu(), same applies here. */
    if (strncmp(curr_cpu->arch.model_name, "QEMU", 4) != 0 || curr_cpu == &boot_cpu) {
        curr_cpu->arch.lapic_freq = calculate_frequency(calculate_lapic_frequency);
    } else {
        curr_cpu->arch.lapic_freq = boot_cpu.arch.lapic_freq;
    }

    /* Sanity check. */
    if (curr_cpu != &boot_cpu) {
        if (curr_cpu->id != lapic_id())
            fatal("CPU ID mismatch (detected %u, LAPIC %u)", curr_cpu->id, lapic_id());
    }

    /* Figure out the timer conversion factor. */
    curr_cpu->arch.lapic_timer_cv = ((curr_cpu->arch.lapic_freq / 8) << 32) / 1000000000;
    kprintf(
        LOG_NOTICE, "lapic: timer conversion factor for CPU %u is %u (freq: %" PRIu64 "MHz)\n",
        curr_cpu->id, curr_cpu->arch.lapic_timer_cv,
        curr_cpu->arch.lapic_freq / 1000000);

    /* Accept all interrupts. */
    lapic_write(LAPIC_REG_TPR, lapic_read(LAPIC_REG_TPR) & 0xffffff00);

    /* Enable the timer: interrupt vector, no extra bits = Unmasked/One-shot. */
    lapic_write(LAPIC_REG_TIMER_INITIAL, 0);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_VECT_TIMER);
}
