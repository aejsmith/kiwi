/* Kiwi x86 low-level interrupt functions
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
 * @brief		Low-level interrupt functions.
 */

#ifndef __ARCH_INTR_H
#define __ARCH_INTR_H

#include <types.h>

/** Various definitions. */
#define INTR_COUNT	256		/**< Total number of interrupts. */
#define FAULT_COUNT	32		/**< Total number of exceptions. */
#define IRQ_COUNT	16		/**< Total number of IRQs. */
#define IRQ_BASE	32		/**< IRQ number base. */

/** Enable interrupts.
 * @return		Previous interrupt state. */
static inline bool intr_enable(void) {
	unative_t flags;

	__asm__ volatile("pushf; sti; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Disable interrupts.
 * @return		Previous interrupt state. */
static inline bool intr_disable(void) {
	unative_t flags;

	__asm__ volatile("pushf; cli; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

/** Restore saved interrupt state.
 * @param state		State to restore. */
static inline void intr_restore(bool state) {
	if(state) {
		__asm__ volatile("sti; nop");
	} else {
		__asm__ volatile("cli");
	}
}

/** Get interrupt state.
 * @return		Current interrupt state. */
static inline bool intr_state(void) {
	unative_t flags;

	__asm__ volatile("pushf; pop %0" : "=r"(flags));
	return (flags & (1<<9)) ? true : false;
}

extern void intr_init(void);
extern void intr_ap_init(void);

#endif /* __ARCH_INTR_H */
