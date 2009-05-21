/* Kiwi error handling functions
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
 * @brief		Error handling functions.
 */

#include <console/console.h>
#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/do_printf.h>

#include <fatal.h>
#include <kdbg.h>
#include <version.h>

/** Atomic variable to protect against nested calls to _fatal(). */
atomic_t fatal_protect = 0;

/** Helper for fatal_printf(). */
static void fatal_printf_helper(char ch, int *total, void *data) {
	console_putch(LOG_FATAL, ch);
	if(ch == '\n') {
		console_putch(LOG_FATAL, ' ');
		console_putch(LOG_FATAL, ' ');
	}
	*total = *total + 1;
}

/** Formatted output function for use during _fatal(). */
static void fatal_printf(const char *format, ...) {
	va_list args;

	va_start(args, format);
	do_printf(fatal_printf_helper, NULL, format, args);
	va_end(args);
}

/** Print an error message and halt the kernel.
 *
 * Prints a formatted error message to the screen, along with a stack trace,
 * then halts the kernel.
 *
 * @param frame		Interrupt stack frame (if any).
 * @param format	The format string for the message.
 * @param ...		The arguments to be used in the formatted message.
 */
void _fatal(intr_frame_t *frame, const char *format, ...) {
	va_list args;

	intr_disable();

	if(atomic_cmp_set(&fatal_protect, 0, 1)) {
		va_start(args, format);
#if CONFIG_SMP
		/* Send an IPI to all other CPUs to halt them. */
		cpu_ipi(IPI_DEST_ALL, 0, IPI_FATAL);
#endif
		console_putch(LOG_FATAL, '\n');
		fatal_printf("Fatal Error (CPU: %u; Version: %s):\n", cpu_current_id(), kiwi_ver_string);
		do_printf(fatal_printf_helper, NULL, format, args);
		console_putch(LOG_FATAL, '\n');

		kdbg_enter(KDBG_ENTRY_FATAL, frame);
	}

	while(1) {
		idle();
	}
}
