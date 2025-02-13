/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 interrupt handling definitions.
 */

#pragma once

#include <arch/frame.h>

#include <x86/descriptor.h>

/** Definitions for hardware exception numbers. */
#define X86_EXCEPTION_DE    0       /**< Divide Error. */
#define X86_EXCEPTION_DB    1       /**< Debug. */
#define X86_EXCEPTION_NMI   2       /**< Non-Maskable Interrupt. */
#define X86_EXCEPTION_BP    3       /**< Breakpoint. */
#define X86_EXCEPTION_OF    4       /**< Overflow. */
#define X86_EXCEPTION_BR    5       /**< BOUND Range Exceeded. */
#define X86_EXCEPTION_UD    6       /**< Invalid Opcode. */
#define X86_EXCEPTION_NM    7       /**< Device Not Available. */
#define X86_EXCEPTION_DF    8       /**< Double Fault. */
#define X86_EXCEPTION_TS    10      /**< Invalid TSS. */
#define X86_EXCEPTION_NP    11      /**< Segment Not Present. */
#define X86_EXCEPTION_SS    12      /**< Stack Fault. */
#define X86_EXCEPTION_GP    13      /**< General Protection Fault. */
#define X86_EXCEPTION_PF    14      /**< Page Fault. */
#define X86_EXCEPTION_MF    16      /**< x87 FPU Floating-Point Error. */
#define X86_EXCEPTION_AC    17      /**< Alignment Check. */
#define X86_EXCEPTION_MC    18      /**< Machine Check. */
#define X86_EXCEPTION_XM    19      /**< SIMD Floating-Point. */

/** Interrupt handler function type. */
typedef void (*interrupt_handler_t)(frame_t *frame);

extern interrupt_handler_t interrupt_table[IDT_ENTRY_COUNT];

extern void interrupt_init(void);
