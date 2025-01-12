/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 interrupt frame structure.
 */

#pragma once

#ifndef __ASM__

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct frame {
    unsigned long spsr;
    unsigned long ip; /* elr */
    unsigned long x30;
    unsigned long sp;
    unsigned long x28;
    unsigned long x29;
    unsigned long x26;
    unsigned long x27;
    unsigned long x24;
    unsigned long x25;
    unsigned long x22;
    unsigned long x23;
    unsigned long x20;
    unsigned long x21;
    unsigned long x18;
    unsigned long x19;
    unsigned long x16;
    unsigned long x17;
    unsigned long x14;
    unsigned long x15;
    unsigned long x12;
    unsigned long x13;
    unsigned long x10;
    unsigned long x11;
    unsigned long x8;
    unsigned long x9;
    unsigned long x6;
    unsigned long x7;
    unsigned long x4;
    unsigned long x5;
    unsigned long x2;
    unsigned long x3;
    unsigned long x0;
    unsigned long x1;
} __packed frame_t;

/** Structure containing a stack frame. */
typedef struct stack_frame {
    ptr_t next;                 /**< Address of next stack frame. */
    ptr_t addr;                 /**< Function return address. */
} stack_frame_t;

/** Return whether an interrupt frame is from user mode.
 * @param frame         Frame to check.
 * @return              Whether the frame is from user mode. */
static inline bool frame_from_user(frame_t *frame) {
    /* SPSR.M[3:2] == PSTATE.EL */
    return (frame->spsr & 0xc) == 0;
}

#endif /* __ASM__ */

#define FRAME_OFF_spsr  (8 * 0)
#define FRAME_OFF_ip    (8 * 1)
#define FRAME_OFF_x30   (8 * 2)
#define FRAME_OFF_sp    (8 * 3)
#define FRAME_OFF_x28   (8 * 4)
#define FRAME_OFF_x29   (8 * 5)
#define FRAME_OFF_x26   (8 * 6)
#define FRAME_OFF_x27   (8 * 7)
#define FRAME_OFF_x24   (8 * 8)
#define FRAME_OFF_x25   (8 * 9)
#define FRAME_OFF_x22   (8 * 10)
#define FRAME_OFF_x23   (8 * 11)
#define FRAME_OFF_x20   (8 * 12)
#define FRAME_OFF_x21   (8 * 13)
#define FRAME_OFF_x18   (8 * 14)
#define FRAME_OFF_x19   (8 * 15)
#define FRAME_OFF_x16   (8 * 16)
#define FRAME_OFF_x17   (8 * 17)
#define FRAME_OFF_x14   (8 * 18)
#define FRAME_OFF_x15   (8 * 19)
#define FRAME_OFF_x12   (8 * 20)
#define FRAME_OFF_x13   (8 * 21)
#define FRAME_OFF_x10   (8 * 22)
#define FRAME_OFF_x11   (8 * 23)
#define FRAME_OFF_x8    (8 * 24)
#define FRAME_OFF_x9    (8 * 25)
#define FRAME_OFF_x6    (8 * 26)
#define FRAME_OFF_x7    (8 * 27)
#define FRAME_OFF_x4    (8 * 28)
#define FRAME_OFF_x5    (8 * 29)
#define FRAME_OFF_x2    (8 * 30)
#define FRAME_OFF_x3    (8 * 31)
#define FRAME_OFF_x0    (8 * 32)
#define FRAME_OFF_x1    (8 * 33)
