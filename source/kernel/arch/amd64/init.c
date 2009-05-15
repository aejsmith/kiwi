/* Kiwi x86 initialization code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		x86 initialization code.
 */

#include <arch/acpi.h>
#include <arch/apic.h>
#include <arch/asm.h>
#include <arch/defs.h>
#include <arch/features.h>
#include <arch/gdt.h>
#include <arch/intr.h>
#include <arch/io.h>
#include <arch/multiboot.h>
#include <arch/pic.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>

#include <time/timer.h>

#include <fatal.h>

/** Check for a flag in a Multiboot information structure. */
#define CHECK_MB_FLAG(i, f)	\
	if(((i)->flags & (f)) == 0) { \
		fatal("Required flag not set: " #f); \
	}

/** Prototypes for functions that aren't defined in any headers. */
extern void arch_premm_init(multiboot_info_t *info);
extern void arch_postmm_init(multiboot_info_t *info);
extern void arch_final_init(multiboot_info_t *info);
extern void arch_ap_init(void);
extern void arch_reboot(void);

/** External initialization functions. */
extern void page_init(void);
extern void page_late_init(void);
extern void console_late_init(void);

extern clock_source_t pit_clock_source;

/** X86 architecture startup code.
 *
 * Initial startup code for the X86 architecture, run before the memory
 * management subsystem is set up.
 *
 * @param info		Multiboot information pointer.
 */
void arch_premm_init(multiboot_info_t *info) {
	gdt_init();
	intr_init();
	cpu_arch_init(&curr_cpu->arch);

	/* Enable OSFXSR early because memcpy/memset use it on machines that
	 * support it. */
	if(CPU_HAS_FXSR(curr_cpu)) {
		write_cr4(read_cr4() | X86_CR4_OSFXSR);
		fninit();
        }

	/* Check for required Multiboot flags. */
	CHECK_MB_FLAG(info, MB_FLAG_MEMINFO);
	CHECK_MB_FLAG(info, MB_FLAG_MMAP);
	//CHECK_MB_FLAG(info, MB_FLAG_MODULES);
	CHECK_MB_FLAG(info, MB_FLAG_CMDLINE);
}

/** X86 architecture startup code.
 *
 * Second stage startup code for the X86 architecture, run after the memory
 * allocation subsystem is set up.
 *
 * @param info		Multiboot information pointer.
 */
void arch_postmm_init(multiboot_info_t *info) {
	acpi_init();
	pic_init();
	if(!apic_local_init()) {
		if(clock_source_set(&pit_clock_source) != 0) {
			fatal("Could not set PIT clock source");
		}
	}

	/* TODO: Should grab the multiboot info (commandline, modules) that we
	 * require here. */
}

/** X86 architecture startup code.
 *
 * Third stage startup code for the X86 architecture, unmaps the temporary
 * identity mapping used during boot.
 *
 * @param info		Multiboot information pointer.
 */
void arch_final_init(multiboot_info_t *info) {
	console_late_init();
	page_late_init();
}

#if CONFIG_SMP
/** Architecture initialization for an AP. */
void arch_ap_init(void) {
	gdt_init();
	intr_ap_init();
	cpu_arch_init(&curr_cpu->arch);

	if(CPU_HAS_FXSR(curr_cpu)) {
		write_cr4(read_cr4() | X86_CR4_OSFXSR);
		fninit();
        }

	/* Initialize the APIC. */
	if(!apic_local_init()) {
		fatal("APIC initialization failed for CPU %" PRIu32 "\n", curr_cpu->id);
	}
}
#endif

/** Reboot the system. */
void arch_reboot(void) {
	int i;

	/* Try the keyboard controller. */
	out8(0x64, 0xfe);
	for(i = 0; i < 10000000; i++) {
		spin_loop_hint();
	}

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
