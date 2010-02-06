/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		x86 bit operations.
 */

#ifndef __ARCH_BITOPS_H
#define __ARCH_BITOPS_H

#include <types.h>

/** Find first set bit in a native-sized value.
 * @note		Does not check if value is zero, caller should do this.
 * @param value		Value to test.
 * @return		Position of first set bit. */
static inline int bitops_ffs(unative_t value) {
	__asm__ ("bsf %1, %0" : "=r"(value) : "rm"(value) : "cc");
	return (int)value;
}

/** Find first zero bit in a native-sized value.
 * @note		Does not check if all bits are set, caller should do
 *			this.
 * @param value		Value to test.
 * @return		Position of first zero bit. */
static inline int bitops_ffz(unative_t value) {
	__asm__ ("bsf %1, %0" : "=r"(value) : "r"(~value) : "cc");
	return (int)value;
}

/** Find last set bit in a native-sized value.
 * @note		Does not check if value is zero, caller should do this.
 * @param value		Value to test.
 * @return		Position of last set bit. */
static inline int bitops_fls(unative_t value) {
        __asm__ ("bsr %1, %0" : "=r" (value) : "rm"(value) : "cc");
      	return (int)value;
}

#endif /* __ARCH_BITOPS_H */
