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
 * @brief               ARM64 exception handling.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>
#include <arm64/kdb.h>

#include <kernel.h>

extern uint8_t arm64_exception_vectors[];

static arm64_irq_handler_t arm64_irq_handler_func;
static void *arm64_irq_handler_private;

/** Sets the hardware IRQ handler. */
void arm64_set_irq_handler(arm64_irq_handler_t handler, void *private) {
    if (arm64_irq_handler_func)
        fatal("Multiple IRQ handlers installed");

    arm64_irq_handler_func    = handler;
    arm64_irq_handler_private = private;
}

/** Handle an IRQ. */
void arm64_irq_handler(frame_t *frame) {
    if (!arm64_irq_handler_func)
        fatal("Received IRQ without registered IRQ handler");

    arm64_irq_handler_func(arm64_irq_handler_private, frame);
}

/** Handle a synchronous exception. */
void arm64_sync_exception_handler(frame_t *frame) {
    unsigned long esr   = arm64_read_sysreg(esr_el1);
    unsigned long class = ARM64_ESR_EC(esr);

    switch (class) {
        case 0b111100:
            /* BRK instruction (AArch64). */
            // TODO: Should not enter KDB if this came from EL0.
            arm64_kdb_brk_handler(frame);
            break;
        default:
            /* TODO: Proper exception handling. */
            fatal_etc(frame, "Unhandled synchronous exception (class %lu)", class);
            break;
    }
}

/** Unhandled exception. */
void arm64_unhandled_exception_handler(frame_t *frame) {
    fatal_etc(frame, "Unhandled CPU exception");
}

/** Set up exception handling. */
__init_text void arm64_exception_init(void) {
    /* Ensure we run exceptions with current EL SP. */
    arm64_write_sysreg(spsel, 1);

    /* Install exception vectors. */
    arm64_write_sysreg(vbar_el1, (ptr_t)arm64_exception_vectors);
}
