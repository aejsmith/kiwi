/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 Programmable Interval Timer code.
 */

#include <arch/io.h>

#include <x86/lapic.h>
#include <x86/pit.h>

#include <device/irq.h>

#include <kernel.h>
#include <time.h>

static irq_status_t pit_irq(void *data) {
    return (timer_tick()) ? IRQ_PREEMPT : IRQ_HANDLED;
}

static void pit_enable(timer_device_t *device) {
    /* Set channel 0 to mode 3 (square wave generator). */
    uint16_t base = PIT_BASE_FREQUENCY / PIT_TIMER_FREQUENCY;
    out8(PIT_MODE, 0x36);
    out8(PIT_CHAN0, base & 0xff);
    out8(PIT_CHAN0, base >> 8);
}

static void pit_disable(timer_device_t *device) {
    /* After this has been done, the PIT will generate one more IRQ. This is
     * ignored. */
    out8(PIT_MODE, 0x30);
    out8(PIT_CHAN0, 0);
    out8(PIT_CHAN0, 0);
}

static timer_device_ops_t pit_timer_device_ops = {
    .type     = TIMER_DEVICE_PERIODIC,
    .enable   = pit_enable,
    .disable  = pit_disable,
};

static timer_device_t pit_timer_device = {
    .name     = "PIT",
    .priority = 0,
    .ops      = &pit_timer_device_ops,
};

/** Initialize the PIT timer. */
static __init_text void pit_init(void) {
    if (lapic_enabled())
        return;

    time_set_timer_device(&pit_timer_device);
    pit_disable(&pit_timer_device);
    irq_register(root_irq_domain, 0, pit_irq, NULL, NULL, NULL);
}

INITCALL_TYPE(pit_init, INITCALL_TYPE_TIME);
