/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		AMD64 local IRQ state control functions.
 */

#ifndef __ARCH_LIRQ_H
#define __ARCH_LIRQ_H

#include <types.h>

/** Enable IRQ delivery.
 * @return		Previous IRQ state. */
static inline bool local_irq_enable(void) {
	unative_t flags;

	__asm__ volatile("pushf; sti; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Disable IRQ delivery.
 * @return		Previous IRQ state. */
static inline bool local_irq_disable(void) {
	unative_t flags;

	__asm__ volatile("pushf; cli; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Restore saved IRQ state.
 * @param state		State to restore. */
static inline void local_irq_restore(bool state) {
	if(state) {
		__asm__ volatile("sti");
	} else {
		__asm__ volatile("cli");
	}
}

/** Get IRQ state.
 * @return		Current IRQ state. */
static inline bool local_irq_state(void) {
	unative_t flags;

	__asm__ volatile("pushf; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

#endif /* __ARCH_LIRQ_H */
