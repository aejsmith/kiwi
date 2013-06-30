/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		Error handling functions.
 */

#include <arch/frame.h>

#include <kernel/system.h>

#include <lib/printf.h>

#include <mm/safe.h>

#include <security/cap.h>

#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

/** Notifier to be called when a fatal error occurs. */
NOTIFIER_DECLARE(fatal_notifier, NULL);

/** Atomic variable to protect against nested calls to fatal(). */
static atomic_t in_fatal = 0;

/** Helper for fatal_printf(). */
static void fatal_printf_helper(char ch, void *data, int *total) {
	if(debug_console_ops) {
		debug_console_ops->putc(ch);
	}
	if(screen_console_ops) {
		screen_console_ops->putc(ch);
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

/**
 * Handle an unrecoverable kernel error.
 *
 * Halts all CPUs, prints a formatted error message to the console and enters
 * KDB. The function will never return.
 *
 * @param frame		Interrupt stack frame (if any).
 * @param fmt		Error message format string.
 * @param ...		Arguments to substitute into format string.
 */
void _fatal(intr_frame_t *frame, const char *fmt, ...) {
	va_list args;

	local_irq_disable();

	if(atomic_inc(&in_fatal) == 0) {
		/* Run callback functions registered. */
		notifier_run_unlocked(&fatal_notifier, NULL, false);

		fatal_printf("\nFatal Error: ");
		va_start(args, fmt);
		do_printf(fatal_printf_helper, NULL, fmt, args);
		va_end(args);
		fatal_printf("\n");

		kdb_enter(KDB_REASON_FATAL, frame);
	}

	/* Halt the current CPU. */
	arch_cpu_halt();
}

/** Handle failure of an assertion.
 * @param cond		String of the condition that failed.
 * @param file		File name that contained the assertion.
 * @param line		Line number of the assertion. */
void __assert_fail(const char *cond, const char *file, int line) {
	_fatal(NULL, "Assertion `%s' failed\nat %s:%d", cond, file, line);
}

/**
 * Print a fatal error message and halt the system.
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
