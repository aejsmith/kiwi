/*
 * Copyright (C) 2009-2021 Alex Smith
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

/** Type of an initcall function. */
typedef void (*initcall_t)(void);

/**
 * Macro to declare an initialization function.
 *
 * Initcalls are called in the initialization thread, after other CPUs have
 * been booted. They are called in the order that they are in the initcall
 * section.
 */
#define INITCALL(func)  \
    static ptr_t __initcall_##func __section(".init.initcalls") __used = (ptr_t)func

extern initcall_t __initcall_start[], __initcall_end[];

extern void arch_init(void);
extern void arch_reboot(void);
extern void arch_poweroff(void);

extern void platform_init(void);

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
