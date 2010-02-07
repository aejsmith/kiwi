/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IA32 architecture core code.
 */

#include <arch/intr.h>
#include <arch/syscall.h>

#include <proc/syscall.h>

#include <fatal.h>

/** System call handler function.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
static bool syscall_intr_handler(unative_t num, intr_frame_t *frame) {
	bool state = intr_enable();

	frame->ax = syscall_handler((syscall_frame_t *)frame);
	intr_restore(state);
	return false;
}

/** Set up the IA32 system call handler. */
void __init_text syscall_arch_init(void) {
	intr_register(SYSCALL_INT_NO, syscall_intr_handler);
}
