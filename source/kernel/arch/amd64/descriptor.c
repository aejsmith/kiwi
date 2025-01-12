/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 descriptor table functions.
 */

#include <arch/stack.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/interrupt.h>

#include <lib/string.h>

#include <cpu.h>

/** ISR array in entry.S. Each handler is aligned to 16 bytes. */
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Array of GDT descriptors. */
static gdt_entry_t initial_gdt[GDT_ENTRY_COUNT] __aligned(8) = {
    /** NULL descriptor (0x0). */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

    /** Kernel CS (0x8). */
    {
        0xffff,             /**< Limit (low). */
        0,                  /**< Base (low). */
        0x8,                /**< Type (Execute). */
        1,                  /**< S (Code/Data). */
        0,                  /**< DPL (0 - Kernel). */
        1,                  /**< Present. */
        0xf,                /**< Limit (high). */
        1,                  /**< 64-bit Code. */
        0,                  /**< Special. */
        1,                  /**< Granularity. */
        0,                  /**< Base (high). */
    },

    /** Kernel DS (0x10). */
    {
        0xffff,             /**< Limit (low). */
        0,                  /**< Base (low). */
        0x2,                /**< Type (Read/Write). */
        1,                  /**< S (Code/Data). */
        0,                  /**< DPL (0 - Kernel). */
        1,                  /**< Present. */
        0xf,                /**< Limit (high). */
        0,                  /**< Ignored. */
        0,                  /**< Special. */
        1,                  /**< Granularity. */
        0,                  /**< Base (high). */
    },

    /** User DS (0x18). */
    {
        0xffff,             /**< Limit (low). */
        0,                  /**< Base (low). */
        0x2,                /**< Type (Read/Write). */
        1,                  /**< S (Code/Data). */
        3,                  /**< DPL (3 - User). */
        1,                  /**< Present. */
        0xf,                /**< Limit (high). */
        0,                  /**< Ignored. */
        0,                  /**< Special. */
        1,                  /**< Granularity. */
        0,                  /**< Base (high). */
    },

    /** User CS (0x20). */
    {
        0xffff,             /**< Limit (low). */
        0,                  /**< Base (low). */
        0x8,                /**< Type (Execute). */
        1,                  /**< S (Code/Data). */
        3,                  /**< DPL (3 - User). */
        1,                  /**< Present. */
        0xf,                /**< Limit (high). */
        1,                  /**< 64-bit Code. */
        0,                  /**< Special. */
        1,                  /**< Granularity. */
        0,                  /**< Base (high). */
    },

    /** TSS descriptor - filled in by gdt_init(). */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/** Array of IDT entries. */
static idt_entry_t kernel_idt[IDT_ENTRY_COUNT] __aligned(8);

/** Set up the GDT for the current CPU.
 * @param cpu           CPU to initialize for. */
static __init_text void gdt_init(cpu_t *cpu) {
    /* Create a copy of the statically allocated GDT. */
    memcpy(cpu->arch.gdt, initial_gdt, sizeof(initial_gdt));

    /* Set up the TSS descriptor. */
    ptr_t base  = (ptr_t)&cpu->arch.tss;
    size_t size = sizeof(cpu->arch.tss);

    gdt_tss_entry_t *desc = (gdt_tss_entry_t *)&cpu->arch.gdt[KERNEL_TSS / 0x08];
    desc->base0   = base & 0xffffff;
    desc->base1   = ((base) >> 24) & 0xff;
    desc->base2   = ((base) >> 32);
    desc->limit0  = size & 0xffff;
    desc->limit1  = (size >> 16) & 0xf;
    desc->present = 1;
    desc->type    = 0x9;

    /* Set the GDT pointer. */
    x86_lgdt(cpu->arch.gdt, sizeof(cpu->arch.gdt) - 1);

    /* Reload the segment registers. There is a 64-bit far jump instruction but
     * GAS doesn't like it... use LRETQ to reload CS instead. */
    __asm__ volatile(
        "push   %0\n"
        "push   $1f\n"
        "lretq\n"
        "1:\n"
        "mov    %1, %%ss\n"
        "mov    %2, %%ds\n"
        "mov    %2, %%es\n"
        "mov    %2, %%fs\n"
        "mov    %2, %%gs\n"
        :: "i"(KERNEL_CS), "r"(KERNEL_DS), "r"(0));

    /* Although once the thread system is up the GS base is pointed at the
     * architecture thread data, we need curr_cpu to work before that. Our CPU
     * data has a pointer at the start which we can use, so point the GS base at
     * that to begin with. */
    cpu->arch.parent = cpu;
    cpu->arch.thread = NULL;
    x86_write_msr(X86_MSR_GS_BASE, (ptr_t)&cpu->arch);
    x86_write_msr(X86_MSR_KERNEL_GS_BASE, 0);
}

/** Set up the TSS for the current CPU.
 * @param cpu           CPU to initialize for. */
static __init_text void tss_init(cpu_t *cpu) {
    /* Set up the contents of the TSS. Point the first IST entry at the double
     * fault stack. */
    memset(&cpu->arch.tss, 0, sizeof(cpu->arch.tss));
    cpu->arch.tss.ist1 = (ptr_t)cpu->arch.double_fault_stack + KSTACK_SIZE;
    cpu->arch.tss.io_bitmap = 104;

    /* Load the TSS segment into TR. */
    x86_ltr(KERNEL_TSS);
}

/** Initialize descriptor tables for the current CPU.
 * @param cpu           CPU to initialize for. */
__init_text void descriptor_init(cpu_t *cpu) {
    /* Initialize and load the GDT/TSS. */
    gdt_init(cpu);
    tss_init(cpu);

    /* Point the CPU to the global IDT. */
    x86_lidt(kernel_idt, (sizeof(kernel_idt) - 1));
}

/** Initialize the IDT shared by all CPUs. */
__init_text void idt_init(void) {
    /* Fill out the handlers in the IDT. */
    for (size_t i = 0; i < IDT_ENTRY_COUNT; i++) {
        ptr_t addr = (ptr_t)&isr_array[i];

        kernel_idt[i].base0    = (addr & 0xffff);
        kernel_idt[i].base1    = ((addr >> 16) & 0xffff);
        kernel_idt[i].base2    = ((addr >> 32) & 0xffffffff);
        kernel_idt[i].ist      = 0;
        kernel_idt[i].reserved = 0;
        kernel_idt[i].sel      = KERNEL_CS;
        kernel_idt[i].unused   = 0;
        kernel_idt[i].flags    = 0x8e;
    }

    /* In tss_init() we point the first IST entry at the double fault stack.
     * Point the double fault IDT entry at this stack. */
    kernel_idt[X86_EXCEPTION_DF].ist = 1;
}
