/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 local IRQ state control functions.
 */

#pragma once

#include <types.h>

/** Get IRQ state.
 * @return              Current IRQ state. */
static inline bool local_irq_state(void) {
    unsigned long daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    return !(daif & (1<<7));
}

/** Enable IRQ delivery.
 * @return              Previous IRQ state. */
static inline bool local_irq_enable(void) {
    bool curr_state = local_irq_state();
    __asm__ volatile("msr daifclr, #2");
    return curr_state;
}

/** Disable IRQ delivery.
 * @return              Previous IRQ state. */
static inline bool local_irq_disable(void) {
    bool curr_state = local_irq_state();
    __asm__ volatile("msr daifset, #2");
    return curr_state;
}

/** Restore saved IRQ state.
 * @param state         State to restore. */
static inline void local_irq_restore(bool state) {
    if (state) {
        __asm__ volatile("msr daifclr, #2");
    } else {
        __asm__ volatile("msr daifset, #2");
    }
}
