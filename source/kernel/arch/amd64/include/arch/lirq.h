/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 local IRQ state control functions.
 */

#pragma once

#include <types.h>

/** Enable IRQ delivery.
 * @return              Previous IRQ state. */
static inline bool local_irq_enable(void) {
    unsigned long flags;

    __asm__ volatile("pushf; sti; pop %0" : "=r"(flags));
    return (flags & (1 << 9)) ? true : false;
}

/** Disable IRQ delivery.
 * @return              Previous IRQ state. */
static inline bool local_irq_disable(void) {
    unsigned long flags;

    __asm__ volatile("pushf; cli; pop %0" : "=r"(flags));
    return (flags & (1 << 9)) ? true : false;
}

/** Restore saved IRQ state.
 * @param state         State to restore. */
static inline void local_irq_restore(bool state) {
    if (state) {
        __asm__ volatile("sti");
    } else {
        __asm__ volatile("cli");
    }
}

/** Get IRQ state.
 * @return              Current IRQ state. */
static inline bool local_irq_state(void) {
    unsigned long flags;

    __asm__ volatile("pushf; pop %0" : "=r"(flags));
    return (flags & (1 << 9)) ? true : false;
}
