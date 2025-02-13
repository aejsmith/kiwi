/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 interrupt frame structure.
 */

#pragma once

/** Interrupt frame structure offsets. */
#define FRAME_OFF_R15       0x0
#define FRAME_OFF_R14       0x8
#define FRAME_OFF_R13       0x10
#define FRAME_OFF_R12       0x18
#define FRAME_OFF_R11       0x20
#define FRAME_OFF_R10       0x28
#define FRAME_OFF_R9        0x30
#define FRAME_OFF_R8        0x38
#define FRAME_OFF_BP        0x40
#define FRAME_OFF_SI        0x48
#define FRAME_OFF_DI        0x50
#define FRAME_OFF_DX        0x58
#define FRAME_OFF_CX        0x60
#define FRAME_OFF_BX        0x68
#define FRAME_OFF_AX        0x70
#define FRAME_OFF_NUM       0x78
#define FRAME_OFF_ERR_CODE  0x80
#define FRAME_OFF_IP        0x88
#define FRAME_OFF_CS        0x90
#define FRAME_OFF_FLAGS     0x98
#define FRAME_OFF_SP        0xa0
#define FRAME_OFF_SS        0xa8

#define FRAME_SIZE          0xb0

#ifndef __ASM__

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct frame {
    unsigned long r15;          /**< R15. */
    unsigned long r14;          /**< R14. */
    unsigned long r13;          /**< R13. */
    unsigned long r12;          /**< R12. */
    unsigned long r11;          /**< R11. */
    unsigned long r10;          /**< R10. */
    unsigned long r9;           /**< R9. */
    unsigned long r8;           /**< R8. */
    unsigned long bp;           /**< RBP. */
    unsigned long si;           /**< RSI. */
    unsigned long di;           /**< RDI. */
    unsigned long dx;           /**< RDX. */
    unsigned long cx;           /**< RCX. */
    unsigned long bx;           /**< RBX. */
    unsigned long ax;           /**< RAX. */
    unsigned long num;          /**< Interrupt number. */
    unsigned long err_code;     /**< Error code (if applicable). */
    unsigned long ip;           /**< IP. */
    unsigned long cs;           /**< CS. */
    unsigned long flags;        /**< FLAGS. */
    unsigned long sp;           /**< SP. */
    unsigned long ss;           /**< SS. */
} __packed frame_t;

/** Return whether an interrupt frame is from user mode.
 * @param frame         Frame to check.
 * @return              Whether the frame is from user mode. */
static inline bool frame_from_user(frame_t *frame) {
    return frame->cs & 3;
}

/** Structure containing a stack frame. */
typedef struct stack_frame {
    ptr_t next;                 /**< Address of next stack frame. */
    ptr_t addr;                 /**< Function return address. */
} stack_frame_t;

#endif /* __ASM__ */
