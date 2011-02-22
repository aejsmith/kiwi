/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Error handling functions.
 */

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <kernel/system.h>

#include <lib/printf.h>

#include <mm/safe.h>

#include <security/cap.h>

#include <console.h>
#include <kdbg.h>
#include <kernel.h>
#include <status.h>

/** Notifier to be called when a fatal error occurs. */
NOTIFIER_DECLARE(fatal_notifier, NULL);

/** Atomic variable to protect against nested calls to _fatal(). */
static atomic_t fatal_protect = 0;

/** Helper for fatal_printf(). */
static void fatal_printf_helper(char ch, void *data, int *total) {
	console_putch(LOG_NONE, ch);
	if(ch == '\n') {
		console_putch(LOG_NONE, ' ');
		console_putch(LOG_NONE, ' ');
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

/** Raise a fatal error.
 *
 * Halts all CPUs, prints a formatted error message to the console and enters
 * KDBG. The function will never return.
 *
 * @param frame		Interrupt stack frame (if any).
 * @param format	The format string for the message.
 * @param ...		The arguments to be used in the formatted message.
 */
void _fatal(intr_frame_t *frame, const char *format, ...) {
	va_list args;

	intr_disable();

	if(atomic_cmp_set(&fatal_protect, 0, 1)) {
#if CONFIG_SMP
		/* Halt all other CPUs. */
		cpu_halt_all();
#endif
		/* Run callback functions registered. */
		notifier_run_unlocked(&fatal_notifier, NULL, false);

		console_putch(LOG_NONE, '\n');
		fatal_printf("Fatal Error (CPU: %u; Version: %s):\n", cpu_current_id(), kiwi_ver_string);
		va_start(args, format);
		do_printf(fatal_printf_helper, NULL, format, args);
		va_end(args);
		console_putch(LOG_NONE, '\n');

		kdbg_enter(KDBG_ENTRY_FATAL, frame);
	}

	/* Halt the current CPU. */
	cpu_halt();
}

/** Print a fatal error message and halt the system.
 *
 * Prints a fatal error message and halts the system. The calling process must
 * have the CAP_FATAL capability.
 *
 * @param message	Message to print.
 */
void kern_fatal(const char *message) {
	char *kmessage;

	if(!cap_check(NULL, CAP_FATAL)) {
		return;
	} else if(strdup_from_user(message, &kmessage) != STATUS_SUCCESS) {
		return;
	}

	fatal("%s", message);
}
