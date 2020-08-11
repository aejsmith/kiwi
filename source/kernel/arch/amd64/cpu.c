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
 * @brief               AMD64 CPU management.
 */

#include <arch/io.h>
#include <arch/page.h>
#include <arch/stack.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/interrupt.h>
#include <x86/lapic.h>
#include <x86/tsc.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <pc/pit.h>

#include <cpu.h>
#include <kdb.h>
#include <kernel.h>

extern void syscall_entry(void);

/** Number of times to get a frequency (must be odd). */
#define FREQUENCY_ATTEMPTS  9

/** Double fault handler stack for the boot CPU. */
static uint8_t boot_doublefault_stack[KSTACK_SIZE] __aligned(PAGE_SIZE);

/** Feature set present on all CPUs. */
x86_features_t cpu_features;

/**
 * Get the current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on. This function should
 * only be used in cases where the curr_cpu variable is unavailable or unsafe.
 * Anywhere else you should be using curr_cpu->id.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_id(void) {
    return (cpu_id_t)lapic_id();
}

/** Dump information about a CPU.
 * @param cpu           CPU to dump. */
void cpu_dump(cpu_t *cpu) {
    kprintf(LOG_NOTICE, " cpu%" PRIu32 ": %s (family: %u, model: %u, stepping: %u)\n",
        cpu->id, cpu->arch.model_name, cpu->arch.family,
        cpu->arch.model, cpu->arch.stepping);
    kprintf(LOG_NOTICE, "  cpu_freq:    %" PRIu64 "MHz\n", cpu->arch.cpu_freq / 1000000);

    if (lapic_enabled())
        kprintf(LOG_NOTICE, "  lapic_freq:  %" PRIu64 "MHz\n", cpu->arch.lapic_freq / 1000000);

    kprintf(LOG_NOTICE, "  cache_align: %d\n", cpu->arch.cache_alignment);
    kprintf(LOG_NOTICE, "  phys_bits:   %d\n", cpu->arch.max_phys_bits);
    kprintf(LOG_NOTICE, "  virt_bits:   %d\n", cpu->arch.max_virt_bits);
}

/** Perform early initialization common to all CPUs. */
__init_text void arch_cpu_early_init(void) {
    /* Initialize the global IDT and the interrupt handler table. */
    idt_init();
    interrupt_init();
}

/** Comparison function for qsort() on an array of uint64_t's.
 * @param a             Pointer to first value.
 * @param b             Pointer to second value.
 * @return              Result of the comparison. */
static __init_text int frequency_compare(const void *a, const void *b) {
    return *(const uint64_t *)a - *(const uint64_t *)b;
}

/** Calculate a frequency multiple times and get the median of the results.
 * @param func          Function to call to get a frequency.
 * @return              Median of the results. */
__init_text uint64_t calculate_frequency(uint64_t (*func)()) {
    uint64_t results[FREQUENCY_ATTEMPTS];
    size_t i;

    /* Get the frequencies. */
    for (i = 0; i < FREQUENCY_ATTEMPTS; i++)
        results[i] = func();

    /* Sort them in ascending order. */
    qsort(results, FREQUENCY_ATTEMPTS, sizeof(uint64_t), frequency_compare);

    /* Pick the median of the results. */
    return results[FREQUENCY_ATTEMPTS / 2];
}

/** Function to calculate the CPU frequency.
 * @return              Calculated frequency. */
static __init_text uint64_t calculate_cpu_frequency(void) {
    uint16_t shi, slo, ehi, elo, ticks;
    uint64_t start, end, cycles;

    /* First set the PIT to rate generator mode. */
    out8(0x43, 0x34);
    out8(0x40, 0xff);
    out8(0x40, 0xff);

    /* Wait for the cycle to begin. */
    do {
        out8(0x43, 0x00);
        slo = in8(0x40);
        shi = in8(0x40);
    } while (shi != 0xff);

    /* Get the start TSC value. */
    start = x86_rdtsc();

    /* Wait for the high byte to drop to 128. */
    do {
        out8(0x43, 0x00);
        elo = in8(0x40);
        ehi = in8(0x40);
    } while (ehi > 0x80);

    /* Get the end TSC value. */
    end = x86_rdtsc();

    /* Calculate the differences between the values. */
    cycles = end - start;
    ticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

    /* Calculate frequency. */
    return (cycles * PIT_BASE_FREQUENCY) / ticks;
}

/** Detect CPU features/information.
 * @param cpu           Pointer to CPU structure for this CPU.
 * @param features      Pointer to the features structure to fill in. */
static __init_text void detect_cpu_features(cpu_t *cpu, x86_features_t *features) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t *ptr;
    size_t i, j;
    char *str;

    /* Get the highest supported standard level. */
    x86_cpuid(X86_CPUID_VENDOR_ID, &features->highest_standard, &ebx, &ecx, &edx);
    if (features->highest_standard < X86_CPUID_FEATURE_INFO)
        fatal("CPUID feature information not supported");

    /* Get standard feature information. */
    x86_cpuid(X86_CPUID_FEATURE_INFO, &eax, &ebx, &features->standard_ecx, &features->standard_edx);

    /* Save model information. */
    cpu->arch.family = (eax >> 8) & 0x0f;
    cpu->arch.model = (eax >> 4) & 0x0f;
    cpu->arch.stepping = eax & 0x0f;

    /* If the CLFLUSH instruction is supported, get the cache line size. If it
     * is not, a sensible default will be chosen later. */
    if (features->clfsh)
        cpu->arch.cache_alignment = ((ebx >> 8) & 0xff) * 8;

    /* Get the highest supported extended level. */
    x86_cpuid(X86_CPUID_EXT_MAX, &features->highest_extended, &ebx, &ecx, &edx);
    if (features->highest_extended & (1<<31)) {
        if (features->highest_extended >= X86_CPUID_EXT_FEATURE) {
            /* Get extended feature information. */
            x86_cpuid(
                X86_CPUID_EXT_FEATURE,
                &eax,
                &ebx,
                &features->extended_ecx,
                &features->extended_edx);
        }

        if (features->highest_extended >= X86_CPUID_BRAND_STRING3) {
            /* Get brand information. */
            ptr = (uint32_t *)cpu->arch.model_name;
            x86_cpuid(X86_CPUID_BRAND_STRING1, &ptr[0], &ptr[1], &ptr[2],  &ptr[3]);
            x86_cpuid(X86_CPUID_BRAND_STRING2, &ptr[4], &ptr[5], &ptr[6],  &ptr[7]);
            x86_cpuid(X86_CPUID_BRAND_STRING3, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

            /* Some CPUs right-justify the string... */
            str = cpu->arch.model_name;
            i = 0; j = 0;
            while (str[i] == ' ')
                i++;
            if (i > 0) {
                while (str[i])
                    str[j++] = str[i++];
                while (j < sizeof(cpu->arch.model_name))
                    str[j++] = 0;
            }
        }

        if (features->highest_extended >= X86_CPUID_ADDRESS_SIZE) {
            /* Get address size information. */
            x86_cpuid(X86_CPUID_ADDRESS_SIZE, &eax, &ebx, &ecx, &edx);
            cpu->arch.max_phys_bits = eax & 0xff;
            cpu->arch.max_virt_bits = (eax >> 8) & 0xff;
        }
    } else {
        features->highest_extended = 0;
    }

    /* Get a brand string if one wasn't found. */
    if (!cpu->arch.model_name[0])
        strcpy(cpu->arch.model_name, "Unknown Model");

    /* If the cache line/address sizes are not set, use a sane default. */
    if (!cpu->arch.cache_alignment)
        cpu->arch.cache_alignment = 64;
    if (!cpu->arch.max_phys_bits)
        cpu->arch.max_phys_bits = 32;
    if (!cpu->arch.max_virt_bits)
        cpu->arch.max_virt_bits = 48;
}

/** Initialize SYSCALL/SYSRET MSRs. */
static __init_text void syscall_init(void) {
    uint64_t fmask, lstar, star;

    /* Disable interrupts and clear direction flag upon entry. */
    fmask = X86_FLAGS_IF | X86_FLAGS_DF;

    /* Set system call entry address. */
    lstar = (uint64_t)syscall_entry;

    /* Set segments for entry and returning. The following happens upon
     * entry to kernel-mode:
     *  - CS is set to the value in IA32_STAR[47:32].
     *  - SS is set to the value in IA32_STAR[47:32] + 8.
     * Upon return to user mode, the following happens:
     *  - CS is set to (the value in IA32_STAR[63:48] + 16).
     *  - SS is set to (the value in IA32_STAR[63:48] + 8).
     * Weird. This means that we have to have a specific GDT order to make
     * things work. We set the SYSRET values below to the kernel DS, so that we
     * get the correct segment (kernel DS + 16 = user CS, and kernel DS + 8 =
     * user DS). */
    star = ((uint64_t)(KERNEL_DS | 0x03) << 48) | ((uint64_t)KERNEL_CS << 32);

    /* Set System Call Enable (SCE) in EFER and write everything out. */
    x86_write_msr(X86_MSR_EFER, x86_read_msr(X86_MSR_EFER) | X86_EFER_SCE);
    x86_write_msr(X86_MSR_FMASK, fmask);
    x86_write_msr(X86_MSR_LSTAR, lstar);
    x86_write_msr(X86_MSR_STAR, star);
}

/** Detect and set up the current CPU.
 * @param cpu           CPU structure for the current CPU. */
__init_text void arch_cpu_early_init_percpu(cpu_t *cpu) {
    x86_features_t features;

    /* If this is the boot CPU, a double fault stack will not have been
     * allocated. Use the pre-allocated one in this case. */
    if (cpu == &boot_cpu)
        cpu->arch.double_fault_stack = boot_doublefault_stack;

    /* Initialize and load descriptor tables. */
    descriptor_init(cpu);

    /* Detect CPU features and information. */
    detect_cpu_features(cpu, &features);

    /* If this is the boot CPU, copy features to the global features structure.
     * Otherwise, check that the feature set matches the global features. We do
     * not allow SMP configurations with different features on different CPUs. */
    if (cpu == &boot_cpu) {
        memcpy(&cpu_features, &features, sizeof(cpu_features));

        /* Check for required features. It is almost certain that AMD64 CPUs
         * will support these, however I cannot find anything in the Intel/AMD
         * manuals that state there is a guaranteed minimum feature set when
         * 64-bit mode is supported, so check to be on the safe side. */
        if (features.highest_standard < X86_CPUID_FEATURE_INFO) {
            fatal("CPUID feature information is not supported");
        } else if (!cpu_features.fpu || !cpu_features.fxsr) {
            fatal("CPU does not support FPU/FXSR");
        } else if (!cpu_features.tsc) {
            fatal("CPU does not support TSC");
        } else if (!cpu_features.pge) {
            fatal("CPU does not support PGE");
        }
    } else {
        if (cpu_features.highest_standard != features.highest_standard ||
            cpu_features.highest_extended != features.highest_extended ||
            cpu_features.standard_edx != features.standard_edx ||
            cpu_features.standard_ecx != features.standard_ecx ||
            cpu_features.extended_edx != features.extended_edx ||
            cpu_features.extended_ecx != features.extended_ecx)
        {
            fatal("CPU %u has different feature set to boot CPU", cpu->id);
        }
    }

    /* Find out the CPU frequency. When running under QEMU the boot CPU's
     * frequency is OK but the others will usually get rubbish, so as a
     * workaround use the boot CPU's frequency on all CPUs under QEMU. */
    if (strncmp(cpu->arch.model_name, "QEMU", 4) != 0 || cpu == &boot_cpu) {
        cpu->arch.cpu_freq = calculate_frequency(calculate_cpu_frequency);
    } else {
        cpu->arch.cpu_freq = boot_cpu.arch.cpu_freq;
    }

    /* Work out the cycles per µs. */
    cpu->arch.cycles_per_us = cpu->arch.cpu_freq / 1000000;

    /* Enable PGE/OSFXSR. */
    x86_write_cr4(x86_read_cr4() | X86_CR4_PGE | X86_CR4_OSFXSR);

    /* Set WP/NE/MP/TS in CR0 (Write Protect, Numeric Error, Monitor
     * Coprocessor, Task Switch), and clear EM (Emulation). TS is set because we
     * do not want the FPU to be enabled initially. */
    x86_write_cr0(
        (x86_read_cr0() | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP | X86_CR0_TS) & ~X86_CR0_EM);

    /* Set up SYSCALL/SYSRET MSRs. */
    syscall_init();

    /* Configure the TSC offset for system_time(). */
    tsc_init_target();
}

/** Display a list of running CPUs.
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @return              KDB status code. */
static kdb_status_t kdb_cmd_cpus(int argc, char **argv, kdb_filter_t *filter) {
    size_t i;

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints a list of all CPUs and information about them.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("ID   Freq (MHz) LAPIC Freq (MHz) Cache Align Model Name\n");
    kdb_printf("==   ========== ================ =========== ==========\n");

    for (i = 0; i <= highest_cpu_id; i++) {
        if (!cpus[i])
            continue;

        kdb_printf(
            "%-4" PRIu32 " %-10" PRIu64 " %-16" PRIu64 " %-11d %s\n",
            cpus[i]->id, cpus[i]->arch.cpu_freq / 1000000,
            cpus[i]->arch.lapic_freq / 1000000, cpus[i]->arch.cache_alignment,
            (cpus[i]->arch.model_name[0]) ? cpus[i]->arch.model_name : "Unknown");
    }

    return KDB_SUCCESS;
}

/** Perform additional initialization. */
__init_text void arch_cpu_init() {
    kdb_register_command("cpus", "Display a list of CPUs.", kdb_cmd_cpus);

    lapic_init();
}

/** Perform additional initialization of the current CPU. */
__init_text void arch_cpu_init_percpu() {
    lapic_init_percpu();
}
