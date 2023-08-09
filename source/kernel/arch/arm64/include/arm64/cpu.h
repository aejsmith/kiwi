/*
 * Copyright (C) 2009-2023 Alex Smith
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
#define ARM64_ESR_EC_SHIFT      26
#define ARM64_ESR_EC_MASK       (0x3ful << ARM64_ESR_EC_SHIFT)
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
#define ARM64_TCR_IRGN0_WB_WA   (UL(1)<<8)
#define ARM64_TCR_ORGN0_WB_WA   (UL(1)<<10)
#define ARM64_TCR_SH0_INNER     (UL(3)<<12)
#define ARM64_TCR_TG0_4         (UL(0)<<14)
#define ARM64_TCR_T1SZ_SHIFT    16
#define ARM64_TCR_IRGN1_WB_WA   (UL(1)<<24)
#define ARM64_TCR_ORGN1_WB_WA   (UL(1)<<26)
#define ARM64_TCR_SH1_INNER     (UL(3)<<28)
#define ARM64_TCR_TG1_4         (UL(2)<<30)
#define ARM64_TCR_IPS_48        (UL(5)<<32)
#define ARM64_TCR_TBI0          (UL(1)<<37)
#define ARM64_TCR_TBI1          (UL(1)<<38)

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

#endif /* __ASM__ */