/*
 * Copyright (C) 2009-2021 Alex Smith
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
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    return curr_state;
}

/** Disable IRQ delivery.
 * @return              Previous IRQ state. */
static inline bool local_irq_disable(void) {
    bool curr_state = local_irq_state();
    __asm__ volatile("msr daifset, #2" ::: "memory");
    return curr_state;
}

/** Restore saved IRQ state.
 * @param state         State to restore. */
static inline void local_irq_restore(bool state) {
    if (state) {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
    } else {
        __asm__ volatile("msr daifset, #2" ::: "memory");
    }
}
