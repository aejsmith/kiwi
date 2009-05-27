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

#include <arch/x86/features.h>
#include <arch/x86/lapic.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <kdbg.h>

/** Get current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on. This function should
 * only be used in cases where the curr_cpu variable is unavailable, i.e.
 * during thread switching. Normally, you should use curr_cpu->id instead.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_current_id(void) {
        return (cpu_id_t)lapic_id();
}

/** Initialize an x86 CPU information structure.
 *
 * Fills in the given x86 CPU information structure with information about
 * the current CPU.
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
