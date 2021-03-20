/*
 * Copyright (C) 2009-2021 Alex Smith
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

#include <kernel.h>

extern uint8_t arm64_exception_vectors[];

/** Handle a synchronous exception. */
void arm64_sync_exception_handler(frame_t *frame) {
    unsigned long esr   = arm64_read_sysreg(esr_el1);
    unsigned long class = ARM64_ESR_EC(esr);

    /* TODO: Proper exception handling. */
    fatal_etc(frame, "Unhandled synchronous exception (class %lu)", class);
}

/** Set up exception handling. */
__init_text void arm64_exception_init(void) {
    /* Ensure we run exceptions with current EL SP. */
    arm64_write_sysreg(spsel, 1);

    /* Install exception vectors. */
    arm64_write_sysreg(vbar_el1, (ptr_t)arm64_exception_vectors);
}
