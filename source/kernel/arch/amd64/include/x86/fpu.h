/*
 * Copyright (C) 2009-2014 Alex Smith
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
 * @brief               AMD64 FPU functions.
 */

#ifndef __X86_FPU_H
#define __X86_FPU_H

#include <x86/cpu.h>

/** Bits in the FPU status register. */
#define X86_FPU_STATUS_IE   (1<<0)      /**< Invalid Operation. */
#define X86_FPU_STATUS_DE   (1<<1)      /**< Denormalized Operand. */
#define X86_FPU_STATUS_ZE   (1<<2)      /**< Zero Divide. */
#define X86_FPU_STATUS_OE   (1<<3)      /**< Overflow. */
#define X86_FPU_STATUS_UE   (1<<4)      /**< Underflow. */
#define X86_FPU_STATUS_PE   (1<<5)      /**< Precision. */

/** Save FPU state.
 * @param buf           Buffer to save into. */
static inline void x86_fpu_save(char buf[512]) {
    __asm__ __volatile__("fxsave (%0)" :: "r"(buf));
}

/** Restore FPU state.
 * @param buf           Buffer to restore from. */
static inline void x86_fpu_restore(char buf[512]) {
    __asm__ __volatile__("fxrstor (%0)" :: "r"(buf));
}

/** Check whether the FPU is enabled.
 * @return              Whether the FPU is enabled. */
static inline bool x86_fpu_state(void) {
    return !(x86_read_cr0() & X86_CR0_TS);
}

/** Enable FPU usage. */
static inline void x86_fpu_enable(void) {
    x86_write_cr0(x86_read_cr0() & ~X86_CR0_TS);
}

/** Disable FPU usage. */
static inline void x86_fpu_disable(void) {
    x86_write_cr0(x86_read_cr0() | X86_CR0_TS);
}

/** Reset the FPU state. */
static inline void x86_fpu_init(void) {
    __asm__ __volatile__("fninit");
}

/** Read the FPU control word.
 * @return              FPU control word. */
static inline uint16_t x86_fpu_cwd(void) {
    uint16_t cwd;

    __asm__ __volatile__("fnstcw %0" : "+m" (cwd));
    return cwd;
}

/** Read the FPU status word.
 * @return              FPU status word. */
static inline uint16_t x86_fpu_swd(void) {
    uint16_t swd;

    __asm__ __volatile__("fnstsw %0" : "+m" (swd));
    return swd;
}

/** Read the MXCSR register.
 * @return              MXCSR register. */
static inline uint32_t x86_fpu_mxcsr(void) {
    uint32_t mxcsr;

    __asm__ __volatile__("stmxcsr %0" : "+m" (mxcsr));
    return mxcsr;
}

#endif /* __X86_FPU_H */
