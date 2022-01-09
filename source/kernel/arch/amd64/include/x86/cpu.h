/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               x86 CPU management.
 */

#pragma once

/** Flags in the CR0 Control Register. */
#define X86_CR0_PE              (1<<0)      /**< Protected Mode Enable. */
#define X86_CR0_MP              (1<<1)      /**< Monitor Coprocessor. */
#define X86_CR0_EM              (1<<2)      /**< Emulation. */
#define X86_CR0_TS              (1<<3)      /**< Task Switched. */
#define X86_CR0_ET              (1<<4)      /**< Extension Type. */
#define X86_CR0_NE              (1<<5)      /**< Numeric Error. */
#define X86_CR0_WP              (1<<16)     /**< Write Protect. */
#define X86_CR0_AM              (1<<18)     /**< Alignment Mask. */
#define X86_CR0_NW              (1<<29)     /**< Not Write-through. */
#define X86_CR0_CD              (1<<30)     /**< Cache Disable. */
#define X86_CR0_PG              (1<<31)     /**< Paging Enable. */

/** Flags in the CR4 Control Register. */
#define X86_CR4_VME             (1<<0)      /**< Virtual-8086 Mode Extensions. */
#define X86_CR4_PVI             (1<<1)      /**< Protected Mode Virtual Interrupts. */
#define X86_CR4_TSD             (1<<2)      /**< Time Stamp Disable. */
#define X86_CR4_DE              (1<<3)      /**< Debugging Extensions. */
#define X86_CR4_PSE             (1<<4)      /**< Page Size Extensions. */
#define X86_CR4_PAE             (1<<5)      /**< Physical Address Extension. */
#define X86_CR4_MCE             (1<<6)      /**< Machine Check Enable. */
#define X86_CR4_PGE             (1<<7)      /**< Page Global Enable. */
#define X86_CR4_PCE             (1<<8)      /**< Performance-Monitoring Counter Enable. */
#define X86_CR4_OSFXSR          (1<<9)      /**< OS Support for FXSAVE/FXRSTOR. */
#define X86_CR4_OSXMMEXCPT      (1<<10)     /**< OS Support for Unmasked SIMD FPU Exceptions. */
#define X86_CR4_VMXE            (1<<13)     /**< VMX-Enable Bit. */
#define X86_CR4_SMXE            (1<<14)     /**< SMX-Enable Bit. */

/** Flags in the debug status register (DR6). */
#define X86_DR6_B0              (1<<0)      /**< Breakpoint 0 condition detected. */
#define X86_DR6_B1              (1<<1)      /**< Breakpoint 1 condition detected. */
#define X86_DR6_B2              (1<<2)      /**< Breakpoint 2 condition detected. */
#define X86_DR6_B3              (1<<3)      /**< Breakpoint 3 condition detected. */
#define X86_DR6_BD              (1<<13)     /**< Debug register access. */
#define X86_DR6_BS              (1<<14)     /**< Single-stepped. */
#define X86_DR6_BT              (1<<15)     /**< Task switch. */

/** Flags in the debug control register (DR7). */
#define X86_DR7_G0              (1<<1)      /**< Global breakpoint 0 enable. */
#define X86_DR7_G1              (1<<3)      /**< Global breakpoint 1 enable. */
#define X86_DR7_G2              (1<<5)      /**< Global breakpoint 2 enable. */
#define X86_DR7_G3              (1<<7)      /**< Global breakpoint 3 enable. */

/** Definitions for bits in the EFLAGS/RFLAGS register. */
#define X86_FLAGS_CF            (1<<0)      /**< Carry Flag. */
#define X86_FLAGS_ALWAYS1       (1<<1)      /**< Flag that must always be 1. */
#define X86_FLAGS_PF            (1<<2)      /**< Parity Flag. */
#define X86_FLAGS_AF            (1<<4)      /**< Auxilary Carry Flag. */
#define X86_FLAGS_ZF            (1<<6)      /**< Zero Flag. */
#define X86_FLAGS_SF            (1<<7)      /**< Sign Flag. */
#define X86_FLAGS_TF            (1<<8)      /**< Trap Flag. */
#define X86_FLAGS_IF            (1<<9)      /**< Interrupt Enable Flag. */
#define X86_FLAGS_DF            (1<<10)     /**< Direction Flag. */
#define X86_FLAGS_OF            (1<<11)     /**< Overflow Flag. */
#define X86_FLAGS_NT            (1<<14)     /**< Nested Task Flag. */
#define X86_FLAGS_RF            (1<<16)     /**< Resume Flag. */
#define X86_FLAGS_VM            (1<<17)     /**< Virtual-8086 Mode. */
#define X86_FLAGS_AC            (1<<18)     /**< Alignment Check. */
#define X86_FLAGS_VIF           (1<<19)     /**< Virtual Interrupt Flag. */
#define X86_FLAGS_VIP           (1<<20)     /**< Virtual Interrupt Pending Flag. */
#define X86_FLAGS_ID            (1<<21)     /**< ID Flag. */

/** Model Specific Registers. */
#define X86_MSR_TSC             0x10        /**< Time Stamp Counter (TSC). */
#define X86_MSR_APIC_BASE       0x1b        /**< LAPIC base address. */
#define X86_MSR_MTRR_BASE0      0x200       /**< Base of the variable length MTRR base registers. */
#define X86_MSR_MTRR_MASK0      0x201       /**< Base of the variable length MTRR mask registers. */
#define X86_MSR_CR_PAT          0x277       /**< PAT. */
#define X86_MSR_MTRR_DEF_TYPE   0x2ff       /**< Default MTRR type. */
#define X86_MSR_EFER            0xc0000080  /**< Extended Feature Enable register. */
#define X86_MSR_STAR            0xc0000081  /**< System Call Target Address. */
#define X86_MSR_LSTAR           0xc0000082  /**< 64-bit System Call Target Address. */
#define X86_MSR_FMASK           0xc0000084  /**< System Call Flag Mask. */
#define X86_MSR_FS_BASE         0xc0000100  /**< FS segment base register. */
#define X86_MSR_GS_BASE         0xc0000101  /**< GS segment base register. */
#define X86_MSR_KERNEL_GS_BASE  0xc0000102  /**< GS base to switch to with SWAPGS. */

/** EFER MSR flags. */
#define X86_EFER_SCE            (1<<0)      /**< System Call Enable. */
#define X86_EFER_LME            (1<<8)      /**< Long Mode (IA-32e) Enable. */
#define X86_EFER_LMA            (1<<10)     /**< Long Mode (IA-32e) Active. */
#define X86_EFER_NXE            (1<<11)     /**< Execute Disable (XD/NX) Bit Enable. */

/** Standard CPUID function definitions. */
#define X86_CPUID_VENDOR_ID     0x00000000  /**< Vendor ID/Highest Standard Function. */
#define X86_CPUID_FEATURE_INFO  0x00000001  /**< Feature Information. */
#define X86_CPUID_CACHE_DESC    0x00000002  /**< Cache Descriptors. */
#define X86_CPUID_SERIAL_NUM    0x00000003  /**< Processor Serial Number. */
#define X86_CPUID_CACHE_PARMS   0x00000004  /**< Deterministic Cache Parameters. */
#define X86_CPUID_MONITOR_MWAIT 0x00000005  /**< MONITOR/MWAIT Parameters. */
#define X86_CPUID_DTS_POWER     0x00000006  /**< Digital Thermal Sensor and Power Management Parameters. */
#define X86_CPUID_DCA           0x00000009  /**< Direct Cache Access (DCA) Parameters. */
#define X86_CPUID_PERFMON       0x0000000a  /**< Architectural Performance Monitor Features. */
#define X86_CPUID_X2APIC        0x0000000b  /**< x2APIC Features/Processor Topology. */
#define X86_CPUID_XSAVE         0x0000000d  /**< XSAVE Features. */

/** Extended CPUID function definitions. */
#define X86_CPUID_EXT_MAX       0x80000000  /**< Largest Extended Function. */
#define X86_CPUID_EXT_FEATURE   0x80000001  /**< Extended Feature Bits. */
#define X86_CPUID_BRAND_STRING1 0x80000002  /**< Processor Name/Brand String (Part 1). */
#define X86_CPUID_BRAND_STRING2 0x80000003  /**< Processor Name/Brand String (Part 2). */
#define X86_CPUID_BRAND_STRING3 0x80000004  /**< Processor Name/Brand String (Part 3). */
#define X86_CPUID_L2_CACHE      0x80000006  /**< Extended L2 Cache Features. */
#define X86_CPUID_ADVANCED_PM   0x80000007  /**< Advanced Power Management. */
#define X86_CPUID_ADDRESS_SIZE  0x80000008  /**< Virtual/Physical Address Sizes. */

#ifndef __ASM__

#include <types.h>

/** Structure containing x86 CPU feature information. */
typedef struct x86_features {
    uint32_t highest_standard;              /**< Highest standard function. */
    uint32_t highest_extended;              /**< Highest extended function. */

    /** Standard CPUID Features (EDX). */
    union {
        struct {
            unsigned fpu : 1;
            unsigned vme : 1;
            unsigned de : 1;
            unsigned pse : 1;
            unsigned tsc : 1;
            unsigned msr : 1;
            unsigned pae : 1;
            unsigned mce : 1;
            unsigned cx8 : 1;
            unsigned apic : 1;
            unsigned : 1;
            unsigned sep : 1;
            unsigned mtrr : 1;
            unsigned pge : 1;
            unsigned mca : 1;
            unsigned cmov : 1;
            unsigned pat : 1;
            unsigned pse36 : 1;
            unsigned psn : 1;
            unsigned clfsh : 1;
            unsigned : 1;
            unsigned ds : 1;
            unsigned acpi : 1;
            unsigned mmx : 1;
            unsigned fxsr : 1;
            unsigned sse : 1;
            unsigned sse2 : 1;
            unsigned ss : 1;
            unsigned htt : 1;
            unsigned tm : 1;
            unsigned : 1;
            unsigned pbe : 1;
        };
        uint32_t standard_edx;
    };

    /** Standard CPUID Features (ECX). */
    union {
        struct {
            unsigned sse3 : 1;
            unsigned pclmulqdq : 1;
            unsigned dtes64 : 1;
            unsigned monitor : 1;
            unsigned dscpl : 1;
            unsigned vmx : 1;
            unsigned smx : 1;
            unsigned est : 1;
            unsigned tm2 : 1;
            unsigned ssse3 : 1;
            unsigned cnxtid : 1;
            unsigned : 2;
            unsigned fma : 1;
            unsigned cmpxchg16b : 1;
            unsigned xtpr : 1;
            unsigned pdcm : 1;
            unsigned : 2;
            unsigned pcid : 1;
            unsigned dca : 1;
            unsigned sse4_1 : 1;
            unsigned sse4_2 : 1;
            unsigned x2apic : 1;
            unsigned movbe : 1;
            unsigned popcnt : 1;
            unsigned tscd : 1;
            unsigned aes : 1;
            unsigned xsave : 1;
            unsigned osxsave : 1;
            unsigned avx : 1;
            unsigned : 3;
        };
        uint32_t standard_ecx;
    };

    /** Extended CPUID Features (EDX). */
    union {
        struct {
            unsigned : 11;
            unsigned syscall : 1;
            unsigned : 8;
            unsigned xd : 1;
            unsigned : 8;
            unsigned lmode : 1;
        };
        uint32_t extended_edx;
    };

    /** Extended CPUID Features (ECX). */
    union {
        struct {
            unsigned lahf : 1;
            unsigned : 31;
        };
        uint32_t extended_ecx;
    };
} x86_features_t;

extern x86_features_t cpu_features;

#define GEN_READ_REG(name, type) \
    static inline type x86_read_ ## name (void) { \
        type r; \
        __asm__ volatile("mov %%" #name ", %0" : "=r"(r)); \
        return r; \
    }
#define GEN_WRITE_REG(name, type) \
    static inline void x86_write_ ## name (type val) { \
        __asm__ volatile("mov %0, %%" #name :: "r"(val)); \
    }

/** Read the CR0 register.
 * @return              Value of the CR0 register. */
GEN_READ_REG(cr0, unsigned long);

/** Write the CR0 register.
 * @param val           New value of the CR0 register. */
GEN_WRITE_REG(cr0, unsigned long);

/** Read the CR2 register.
 * @return              Value of the CR2 register. */
GEN_READ_REG(cr2, unsigned long);

/** Read the CR3 register.
 * @return              Value of the CR3 register. */
GEN_READ_REG(cr3, unsigned long);

/** Write the CR3 register.
 * @param val           New value of the CR3 register. */
GEN_WRITE_REG(cr3, unsigned long);

/** Read the CR4 register.
 * @return              Value of the CR4 register. */
GEN_READ_REG(cr4, unsigned long);

/** Write the CR4 register.
 * @param val           New value of the CR4 register. */
GEN_WRITE_REG(cr4, unsigned long);

/** Read the DR0 register.
 * @return              Value of the DR0 register. */
GEN_READ_REG(dr0, unsigned long);

/** Write the DR0 register.
 * @param val           New value of the DR0 register. */
GEN_WRITE_REG(dr0, unsigned long);

/** Read the DR1 register.
 * @return              Value of the DR1 register. */
GEN_READ_REG(dr1, unsigned long);

/** Write the DR1 register.
 * @param val           New value of the DR1 register. */
GEN_WRITE_REG(dr1, unsigned long);

/** Read the DR2 register.
 * @return              Value of the DR2 register. */
GEN_READ_REG(dr2, unsigned long);

/** Write the DR2 register.
 * @param val           New value of the DR2 register. */
GEN_WRITE_REG(dr2, unsigned long);

/** Read the DR3 register.
 * @return              Value of the DR3 register. */
GEN_READ_REG(dr3, unsigned long);

/** Write the DR3 register.
 * @param val           New value of the DR3 register. */
GEN_WRITE_REG(dr3, unsigned long);

/** Read the DR6 register.
 * @return              Value of the DR6 register. */
GEN_READ_REG(dr6, unsigned long);

/** Write the DR6 register.
 * @param val           New value of the DR6 register. */
GEN_WRITE_REG(dr6, unsigned long);

/** Read the DR7 register.
 * @return              Value of the DR7 register. */
GEN_READ_REG(dr7, unsigned long);

/** Write the DR7 register.
 * @param val           New value of the DR7 register. */
GEN_WRITE_REG(dr7, unsigned long);

#undef GEN_READ_REG
#undef GEN_WRITE_REG

/** Get current value of EFLAGS/RFLAGS.
 * @return              Current value of EFLAGS/RFLAGS. */
static inline unsigned long x86_read_flags(void) {
    unsigned long val;

    __asm__ volatile("pushf; pop %0" : "=rm"(val));
    return val;
}

/** Set value of EFLAGS/RFLAGS.
 * @param val           New value for EFLAGS/RFLAGS. */
static inline void x86_write_flags(unsigned long val) {
    __asm__ volatile("push %0; popf" :: "rm"(val));
}

/** Read an MSR.
 * @param msr           MSR to read.
 * @return              Value read from the MSR. */
static inline uint64_t x86_read_msr(uint32_t msr) {
    uint32_t low, high;

    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (((uint64_t)high) << 32) | low;
}

/** Write an MSR.
 * @param msr           MSR to write to.
 * @param value         Value to write to the MSR. */
static inline void x86_write_msr(uint32_t msr, uint64_t value) {
    __asm__ volatile("wrmsr" :: "a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

/** Execute the CPUID instruction.
 * @param level         CPUID level.
 * @param a             Where to store EAX value.
 * @param b             Where to store EBX value.
 * @param c             Where to store ECX value.
 * @param d             Where to store EDX value. */
static inline void x86_cpuid(uint32_t level, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "0"(level));
}

/** Invalidate a TLB entry.
 * @param addr          Address to invalidate. */
static inline void x86_invlpg(ptr_t addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(addr));
}

extern uint64_t calculate_frequency(uint64_t (*func)());

#endif /* __ASM__ */
