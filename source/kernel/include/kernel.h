/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Core kernel functions/definitions.
 */

#ifndef __KERNEL_H
#define __KERNEL_H

#include <kernel/system.h>
#include <lib/notifier.h>

struct cpu;
struct intr_frame;

extern notifier_t fatal_notifier;
extern bool shutdown_in_progress;

/** Version information for the kernel, defined in a build-generated file. */
extern int kiwi_ver_release;		/**< Kiwi release number. */
extern int kiwi_ver_update;		/**< Release update number. */
extern int kiwi_ver_revision;		/**< Release revision number. */
extern const char *kiwi_ver_string;	/**< String of version number. */

/** Type of an initcall function. */
typedef void (*initcall_t)(void);

/** Macro to declare an initialisation function.
 * @note		Initcalls are called in the initialisation thread,
 *			after other CPUs have been booted. They are called in
 *			the order that they are in the initcall section. */
#define INITCALL(func)	\
	static ptr_t __initcall_##func __section(".init.initcalls") __used = (ptr_t)func

extern void arch_premm_init(void);
extern void arch_postmm_init(void);
extern void arch_ap_init(struct cpu *cpu);

extern void platform_premm_init(void);
extern void platform_postmm_init(void);

extern void platform_reboot(void);
extern void platform_poweroff(void);

extern void system_shutdown(int action);

extern void _fatal(struct intr_frame *frame, const char *format, ...) __noreturn __printf(2, 3);

/** Print an error message and halt the kernel.
 * @param fmt		The format string for the message.
 * @param ...		The arguments to be used in the formatted message. */
#define fatal(fmt...)	_fatal(NULL, fmt)

#endif /* __KERNEL_H */
