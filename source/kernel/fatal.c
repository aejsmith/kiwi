/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
        arch_kdb_trap_cpus();

        /* Run callback functions registered. */
        notifier_run_unsafe(&fatal_notifier, NULL, false);

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
 *
 * @return              STATUS_PERM_DENIED if the caller does not have
 *                      PRIV_FATAL.
 *                      STATUS_INVALID_ADDR if message is an invalid address.
 */
status_t kern_system_fatal(const char *message) {
    char *kmessage;

    if (!security_check_priv(PRIV_FATAL))
        return STATUS_PERM_DENIED;

    status_t ret = strdup_from_user(message, &kmessage);
    if (ret != STATUS_SUCCESS)
        return ret;

    fatal("%s", message);
}
