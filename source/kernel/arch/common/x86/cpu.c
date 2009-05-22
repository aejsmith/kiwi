/* Kiwi x86 CPU management
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		x86 CPU management.
 */

#include <arch/asm.h>
#include <arch/x86/features.h>
#include <arch/x86/lapic.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/page.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_SMP

extern char __ap_trampoline_start[];
extern char __ap_trampoline_end[];
extern atomic_t ap_boot_wait;

extern void __kernel_ap_entry(void);

/** Stack pointer AP should use during boot. */
void *ap_stack_ptr = 0;

/** Waiting variable for cpu_boot_delay(). */
static atomic_t cpu_boot_delay_wait = 0;

/** CPU boot delay timer handler. */
static bool cpu_boot_delay_func(void) {
	atomic_set(&cpu_boot_delay_wait, 1);
	return false;
}

/** Delay for a number of µseconds during CPU startup.
 * @param us		Number of µseconds to wait. */
static void cpu_boot_delay(uint64_t us) {
	timer_t timer;

	atomic_set(&cpu_boot_delay_wait, 0);

	timer_init(&timer, TIMER_FUNCTION, cpu_boot_delay_func);
	intr_enable();
	timer_start(&timer, us * 1000);

	while(atomic_get(&cpu_boot_delay_wait) == 0);
	intr_disable();
}

/** Boot a CPU.
 *
 * Boots a secondary CPU.
 *
 * @param id		Local APIC ID.
 */
void cpu_boot(cpu_t *cpu) {
	uint32_t delay = 0;
	uint32_t *dest;
	size_t size;
	void *stack;

	kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 " (0x%p)...\n", cpu->id, cpu);
	atomic_set(&ap_boot_wait, 0);

	/* Copy the trampoline code to 0x7000 and set the entry point address. */
	size = (ptr_t)__ap_trampoline_end - (ptr_t)__ap_trampoline_start;
	dest = page_phys_map(0x7000, size, MM_FATAL);
	memcpy(dest, (void *)__ap_trampoline_start, size);
	dest[1] = (uint32_t)KA2PA(__kernel_ap_entry);
	page_phys_unmap(dest, size);

	/* Allocate a new stack and set the CPU structure pointer. */
	stack = kheap_alloc(KSTACK_SIZE, MM_FATAL);
	*(ptr_t *)stack = (ptr_t)cpu;
	ap_stack_ptr = stack + KSTACK_SIZE;

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	lapic_ipi(IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_INIT, 0x00);
	cpu_boot_delay(10000);

	/* Send a SIPI. The 0x07 argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	lapic_ipi(IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	cpu_boot_delay(10000);

	/* If the CPU is up, then return. */
	if(atomic_get(&ap_boot_wait)) {
		return;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	lapic_ipi(IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	while(delay < 5000000) {
		if(atomic_get(&ap_boot_wait)) {
			return;
		}
		cpu_boot_delay(10000);
		delay += 10000;
	}

	fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
}

/** Send an IPI.
 *
 * Sends an IPI (inter-processor interrupt) to the specified processors.
 *
 * @param dest		Where to send the IPI to.
 * @param id		Destination CPU ID for IPI_DEST_SINGLE.
 * @param vector	IPI vector.
 */
void cpu_ipi(uint8_t dest, cpu_id_t id, uint8_t vector) {
	if(vector == IPI_KDBG || vector == IPI_FATAL) {
		lapic_ipi(dest, id, LAPIC_IPI_NMI, 0);
	} else {
		lapic_ipi(dest, id, LAPIC_IPI_FIXED, vector);
	}
}
#endif /* CONFIG_SMP */

/** Get current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_current_id(void) {
        return (cpu_id_t)lapic_id();
}

/** Initialize an x86 CPU information structure.
 *
 * Fills in the given x86 CPU information structure with information about
 * the current CPU. Assumes CPUID is supported - should be checked in the
 * boot code.
 *
 * @param cpu		CPU information structure to fill in.
 */
void cpu_arch_init(cpu_arch_t *cpu) {
	uint32_t eax, ebx, ecx, edx;
	size_t i = 0, j = 0;
	uint32_t *ptr;
	char *str;

	/* Get the highest supported standard level. */
	cpuid(CPUID_VENDOR_ID, &cpu->features.largest_standard, &ebx, &ecx, &edx);
	if(cpu->features.largest_standard >= CPUID_FEATURE_INFO) {
		/* Get standard feature information. */
		cpuid(CPUID_FEATURE_INFO, &eax, &ebx, &cpu->features.feat_ecx, &cpu->features.feat_edx);
		cpu->family = (eax >> 8) & 0x0f;
		cpu->model = (eax >> 4) & 0x0f;
		cpu->stepping = eax & 0x0f;
	}

	/* Get the highest supported extended level. */
	cpuid(CPUID_EXT_MAX, &cpu->features.largest_extended, &ebx, &ecx, &edx);
	if(cpu->features.largest_extended & (1<<31)) {
		if(cpu->features.largest_extended >= CPUID_EXT_FEATURE) {
			/* Get extended feature information. */
			cpuid(CPUID_EXT_FEATURE, &eax, &ebx, &cpu->features.ext_ecx, &cpu->features.ext_edx);
		}

		if(cpu->features.largest_extended >= CPUID_BRAND_STRING3) {
			/* Get brand information. */
			memset(cpu->model_name, 0, sizeof(cpu->model_name));
			str = cpu->model_name;
			ptr = (uint32_t *)str;

			cpuid(CPUID_BRAND_STRING1, &ptr[0], &ptr[1], &ptr[2],  &ptr[3]);
			cpuid(CPUID_BRAND_STRING2, &ptr[4], &ptr[5], &ptr[6],  &ptr[7]);
			cpuid(CPUID_BRAND_STRING3, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

			/* Some CPUs right-justify the string... */
			while(str[i] == ' ') {
				i++;
			}
			if(i > 0) {
				while(str[i]) {
					str[j++] = str[i++];
				}
				while(j < sizeof(cpu->model_name)) {
					str[j++] = 0;
				}
			}
		}
	} else {
		cpu->features.largest_extended = 0;
	}
}

/** CPU information command for KDBG.
 *
 * Prints a list of all CPUs and information about them.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success.
 */
int kdbg_cmd_cpus(int argc, char **argv) {
	size_t i;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_KDBG, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_KDBG, "Prints a list of all CPUs and information about them.\n");
		return KDBG_OK;
	}

	kprintf(LOG_KDBG, "ID   State    Model Name\n");
	kprintf(LOG_KDBG, "==   =====    ==========\n");

	for(i = 0; i <= cpu_id_max; i++) {
		if(cpus[i] == NULL) {
			continue;
		}

		kprintf(LOG_KDBG, "%-4" PRIu32 " ", cpus[i]->id);
		switch(cpus[i]->state) {
		case CPU_DISABLED:	kprintf(LOG_KDBG, "Disabled "); break;
		case CPU_DOWN:		kprintf(LOG_KDBG, "Down     "); break;
		case CPU_RUNNING:	kprintf(LOG_KDBG, "Running  "); break;
		default:		kprintf(LOG_KDBG, "Bad      "); break;
		}
		kprintf(LOG_KDBG, "%s\n", (cpus[i]->arch.model_name[0]) ? cpus[i]->arch.model_name : "Unknown");
	}

	return KDBG_OK;
}
