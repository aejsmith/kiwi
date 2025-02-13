/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 CPU register definitions.
 */

#pragma once

#include <types.h>

/** Current Exception Level values, as contained in CurrentEL */
#define ARM64_CURRENTEL_EL0     (0<<2)
#define ARM64_CURRENTEL_EL1     (1<<2)
#define ARM64_CURRENTEL_EL2     (2<<2)
#define ARM64_CURRENTEL_EL3     (3<<2)

/** Exception Syndrome Register (ESR_ELx). */
#define ARM64_ESR_ISS_SHIFT     0
#define ARM64_ESR_ISS_MASK      (UL(0x1ffffff) << ARM64_ESR_ISS_SHIFT)
#define ARM64_ESR_ISS(esr)      (((esr) & ARM64_ESR_ISS_MASK) >> ARM64_ESR_ISS_SHIFT)
#define ARM64_ESR_EC_SHIFT      26
#define ARM64_ESR_EC_MASK       (UL(0x3f) << ARM64_ESR_EC_SHIFT)
#define ARM64_ESR_EC(esr)       (((esr) & ARM64_ESR_EC_MASK) >> ARM64_ESR_EC_SHIFT)

/** Hypervisor Control Register (HCR_EL2). */
#define ARM64_HCR_RW            (UL(1)<<31)

/** Saved Program Status Register (SPSR_ELx). */
#define ARM64_SPSR_MODE_EL0T    (UL(0)<<0)
#define ARM64_SPSR_MODE_EL1T    (UL(4)<<0)
#define ARM64_SPSR_MODE_EL1H    (UL(5)<<0)
#define ARM64_SPSR_MODE_EL2T    (UL(8)<<0)
#define ARM64_SPSR_MODE_EL2H    (UL(9)<<0)
#define ARM64_SPSR_F            (UL(1)<<6)
#define ARM64_SPSR_I            (UL(1)<<7)
#define ARM64_SPSR_A            (UL(1)<<8)
#define ARM64_SPSR_D            (UL(1)<<9)

/** System Control Register (SCTLR_ELx). */
#define ARM64_SCTLR_M           (UL(1)<<0)
#define ARM64_SCTLR_A           (UL(1)<<1)
#define ARM64_SCTLR_C           (UL(1)<<2)
#define ARM64_SCTLR_I           (UL(1)<<12)
#define ARM64_SCTLR_EL1_RES1    ((UL(1)<<11) | (UL(1)<<20) | (UL(1)<<22) | (UL(1)<<28) | (UL(1)<<29))

/** Translation Control Register (TCR_ELx). */
#define ARM64_TCR_T0SZ_SHIFT    0
#define ARM64_TCR_EPD0          (UL(1)<<7)
#define ARM64_TCR_IRGN0_WB_WA   (UL(1)<<8)
#define ARM64_TCR_ORGN0_WB_WA   (UL(1)<<10)
#define ARM64_TCR_SH0_INNER     (UL(3)<<12)
#define ARM64_TCR_TG0_4         (UL(0)<<14)
#define ARM64_TCR_T1SZ_SHIFT    16
#define ARM64_TCR_A1            (UL(1)<<22)
#define ARM64_TCR_EPD1          (UL(1)<<23)
#define ARM64_TCR_IRGN1_WB_WA   (UL(1)<<24)
#define ARM64_TCR_ORGN1_WB_WA   (UL(1)<<26)
#define ARM64_TCR_SH1_INNER     (UL(3)<<28)
#define ARM64_TCR_TG1_4         (UL(2)<<30)
#define ARM64_TCR_IPS_48        (UL(5)<<32)
#define ARM64_TCR_TBI0          (UL(1)<<37)
#define ARM64_TCR_TBI1          (UL(1)<<38)

/** Translation Table Base Register (TTBR_ELx). */
#define ARM64_TTBR_ASID_SHIFT   48

/** TLBI input value bits. */
#define ARM64_TLBI_VADDR_MASK   ((UL(1)<<44) - 1)
#define ARM64_TLBI_ASID_SHIFT   48

#ifndef __ASM__

/** Read from a system register.
 * @param r             Register name.
 * @return              Register value. */
#define arm64_read_sysreg(r) \
    __extension__ \
    ({ \
        unsigned long __v; \
        __asm__ __volatile__("mrs %0, " STRINGIFY(r) : "=r"(__v)); \
        __v; \
    })

/** Write to a system register.
 * @param r             Register name.
 * @param v             Value to write. */
#define arm64_write_sysreg(r, v) \
    do { \
        unsigned long __v = (unsigned long)(v); \
        __asm__ __volatile__("msr " STRINGIFY(r) ", %0" :: "r"(__v)); \
    } while (0)

/** ISB instruction. */
#define arm64_isb() \
    __asm__ __volatile__("isb");

/** TLBI instruction with no address.
 * @param op            Operation to perform. */
#define arm64_tlbi(op) \
    do { \
        __asm__ volatile("tlbi " STRINGIFY(op)); \
        arm64_isb(); \
    } while (0)

/** TLBI instruction with value.
 * @param op            Operation to perform.
 * @param addr          Address to operate on. */
#define arm64_tlbi_val(op, val) \
    do { \
        __asm__ volatile("tlbi " STRINGIFY(op) ", %0" :: "r"((unsigned long)(val))); \
        arm64_isb(); \
    } while (0)

#endif /* __ASM__ */