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
 * @brief               Kernel debugger.
 */

#pragma once

#include <lib/atomic.h>
#include <lib/notifier.h>
#include <lib/string.h>

#include <kernel.h>
#include <types.h>

/** KDB status codes. */
typedef enum kdb_status {
    KDB_SUCCESS,                    /**< Command completed successfully. */
    KDB_FAILURE,                    /**< Command failed or did not exist. */
    KDB_CONTINUE,                   /**< Command should exit KDB. */
    KDB_STEP,                       /**< Command wants to single-step. */
} kdb_status_t;

/** KDB entry reasons. */
typedef enum kdb_reason {
    KDB_REASON_USER,                /**< Entry upon user request. */
    KDB_REASON_FATAL,               /**< Entry due to fatal error. */
    KDB_REASON_BREAK,               /**< Hit a breakpoint. */
    KDB_REASON_WATCH,               /**< Hit a watchpoint. */
    KDB_REASON_STEP,                /**< Single-stepped. */
} kdb_reason_t;

/** KDB output filter structure. */
typedef struct kdb_filter {
    /** Function for the filter.
     * @param line          Line being output. The function is called a final
     *                      time with a NULL line pointer once the command
     *                      completes, and should free the data if necessary.
     * @param data          Data pointer set for the filter.
     * @return              Whether to output the line. */
    bool (*func)(const char *line, void *data);

    void *data;                     /**< Data for the filter. */
} kdb_filter_t;

/** Type of a KDB command.
 * @param argc          Number of arguments.
 * @param argv          Arguments passed to the command.
 * @param filter        If the command is being used as an output filter,
 *                      points to the filter data structure to fill in.
 * @return              Status code describing what to do. */
typedef kdb_status_t (*kdb_command_t)(int argc, char **argv, kdb_filter_t *filter);

/** KDB backtrace callback type.
 * @param addr          Address of trace entry. */
typedef void (*kdb_backtrace_cb_t)(ptr_t addr);

/** Check if a help message is wanted.
 * @param argc          Number of arguments.
 * @param argv          Arguments passed to the command.
 * @return              Whether a help message is wanted. */
static inline bool kdb_help(int argc, char **argv) {
    return (argc > 1 && strcmp(argv[1], "--help") == 0);
}

struct frame;
struct thread;

extern atomic_t kdb_running;
extern struct frame *curr_kdb_frame;
extern notifier_t kdb_entry_notifier;
extern notifier_t kdb_exit_notifier;

extern int arch_kdb_install_breakpoint(ptr_t addr);
extern int arch_kdb_install_watchpoint(ptr_t addr, size_t size, bool rw);
extern bool arch_kdb_remove_breakpoint(unsigned index);
extern bool arch_kdb_remove_watchpoint(unsigned index);
extern bool arch_kdb_get_breakpoint(unsigned index, ptr_t *_addr);
extern bool arch_kdb_get_watchpoint(unsigned index, ptr_t *_addr, size_t *_size, bool *_rw);
extern void arch_kdb_backtrace(struct thread *thread, kdb_backtrace_cb_t cb);
extern bool arch_kdb_register_value(const char *name, size_t len, unsigned long *_reg);
extern void arch_kdb_dump_registers(void);

#if CONFIG_SMP
extern void arch_kdb_trap_cpus(void);
#else
static inline void arch_kdb_trap_cpus(void) {}
#endif

extern kdb_status_t kdb_main(kdb_reason_t reason, struct frame *frame, unsigned index);
extern void kdb_exception(const char *name, struct frame *frame);

extern void kdb_vprintf(const char *fmt, va_list args);
extern void kdb_printf(const char *fmt, ...) __printf(1, 2);
extern uint16_t kdb_getc(void);
extern void *kdb_malloc(size_t size);
extern void kdb_free(void *addr);
extern kdb_status_t kdb_parse_expression(char *exp, uint64_t *_val, char **_str);

extern void kdb_enter(kdb_reason_t reason, struct frame *frame);

extern void kdb_register_command(const char *name, const char *description, kdb_command_t func);
extern void kdb_unregister_command(const char *name);

extern void arch_kdb_init(void);
extern void kdb_init(void);
