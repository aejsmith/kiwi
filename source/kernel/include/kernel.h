/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Core kernel functions/definitions.
 */

#pragma once

#include <arch/lirq.h>

#include <kernel/log.h>
#include <kernel/system.h>

#include <lib/notifier.h>

struct cpu;
struct frame;
struct kboot_tag;

extern atomic_uint in_fatal;
extern notifier_t fatal_notifier;
extern bool shutdown_in_progress;

/** Version information for the kernel, defined in a build-generated file. */
extern int kiwi_ver_release;        /**< Kiwi release number. */
extern int kiwi_ver_update;         /**< Release update number. */
extern int kiwi_ver_revision;       /**< Release revision number. */
extern const char *kiwi_ver_string; /**< String of version number. */

/**
 * Initialization function types. All functions with the same type get called
 * in indeterminate order at a time specific to that type.
 */
typedef enum initcall_type {
    /** Early platform device detection. */
    INITCALL_TYPE_EARLY_DEVICE,

    /** Register IRQ domains/controllers. */
    INITCALL_TYPE_IRQ,

    /** Register timer devices. */
    INITCALL_TYPE_TIME,

    /** Builtin platform device initialisation. */
    INITCALL_TYPE_DEVICE,

    /** Late initialization functions called on the init thread. */
    INITCALL_TYPE_OTHER,
} initcall_type_t;

/** Type of an initcall function. */
typedef struct initcall {
    initcall_type_t type;
    void (*func)(void);
} initcall_t;

/** Macro to declare an initialization function with a specified type. */
#define INITCALL_TYPE(_func, _type) \
    static initcall_t __initcall_##_func __section(".init.initcalls") __used = { \
        .type = _type, \
        .func = _func, \
     }

/** Macro to declare an initialization function as INITCALL_TYPE_OTHER. */
#define INITCALL(_func) \
    INITCALL_TYPE(_func, INITCALL_TYPE_OTHER)

extern void initcall_run(initcall_type_t type);

extern void arch_init(void);
extern void arch_reboot(void);
extern void arch_poweroff(void);

extern void update_boot_progress(int percent);

extern void system_shutdown(unsigned action);

extern void fatal_etc(struct frame *frame, const char *fmt, ...) __noreturn __printf(2, 3);

/** Handle an unrecoverable kernel error.
 * @param fmt           Error message format string.
 * @param ...           Arguments to substitute into format string. */
#define fatal(fmt...)   fatal_etc(NULL, fmt)

#define fatal_todo() fatal("TODO: %s", __func__)

extern int kvprintf(int level, const char *fmt, va_list args);
extern int kprintf(int level, const char *fmt, ...) __printf(2, 3);

extern void log_early_init(void);

extern void preempt_disable(void);
extern void preempt_enable(void);

extern void kmain(uint32_t magic, struct kboot_tag *tags);
extern void kmain_secondary(struct cpu *cpu);
