/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Error handling functions.
 */

#include <arch/frame.h>

#include <kernel/system.h>

#include <lib/printf.h>

#include <mm/safe.h>

#include <security/security.h>

#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

/** Notifier to be called when a fatal error occurs. */
NOTIFIER_DEFINE(fatal_notifier, NULL);

/** Atomic variable to protect against nested calls to fatal(). */
atomic_uint in_fatal;

/** Helper for fatal_printf(). */
static void fatal_printf_helper(char ch, void *data, int *total) {
    if (debug_console.out)
        debug_console.out->putc_unsafe(ch);
    if (main_console.out)
        main_console.out->putc_unsafe(ch);

    kboot_log_write(ch);

    *total = *total + 1;
}

/**
 * Handles an unrecoverable kernel error. Halts all CPUs, prints a formatted
 * error message to the console and enters KDB. The function will never return.
 *
 * @param frame         Interrupt stack frame (if any).
 * @param fmt           Error message format string.
 * @param ...           Arguments to substitute into format string.
 */
void fatal_etc(frame_t *frame, const char *fmt, ...) {
    local_irq_disable();

    if (atomic_fetch_add(&in_fatal, 1) == 0) {
        /* Run callback functions registered. */
        notifier_run_unsafe(&fatal_notifier, NULL, false);

        arch_kdb_trap_cpus();

        do_printf(fatal_printf_helper, NULL, "\nFATAL: ");

        va_list args;
        va_start(args, fmt);
        do_vprintf(fatal_printf_helper, NULL, fmt, args);
        va_end(args);

        do_printf(fatal_printf_helper, NULL, "\n");

        kdb_enter(KDB_REASON_FATAL, frame);
    }

    /* Halt the current CPU. */
    arch_cpu_halt();
}

void __assert_fail(const char *cond, const char *file, int line) {
    fatal("Assertion `%s' failed\nat %s:%d", cond, file, line);
}

/**
 * Prints a fatal error message and halts the system. The calling process must
 * have the PRIV_FATAL privilege.
 *
 * @param message       Message to print.
 */
void kern_system_fatal(const char *message) {
    char *kmessage;

    if (!security_check_priv(PRIV_FATAL))
        return;

    if (strdup_from_user(message, &kmessage) != STATUS_SUCCESS)
        return;

    fatal("%s", message);
}
