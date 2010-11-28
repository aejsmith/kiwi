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
 * @brief		Core kernel functions/definitions.
 */

#ifndef __KERNEL_H
#define __KERNEL_H

#include <lib/notifier.h>
#include <kargs.h>

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

extern void arch_premm_init(kernel_args_t *args);
extern void arch_postmm_init(kernel_args_t *args);
extern void arch_ap_init(kernel_args_t *args, struct cpu *cpu);

extern void platform_premm_init(kernel_args_t *args);
extern void platform_postmm_init(kernel_args_t *args);

extern void platform_reboot(void);
extern void platform_poweroff(void);

extern void _fatal(struct intr_frame *frame, const char *format, ...) __noreturn __printf(2, 3);

/** Print an error message and halt the kernel.
 * @param fmt		The format string for the message.
 * @param ...		The arguments to be used in the formatted message. */
#define fatal(fmt...)	_fatal(NULL, fmt)

#endif /* __KERNEL_H */
