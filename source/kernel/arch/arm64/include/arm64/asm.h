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
 * @brief               ARM64 assembly code definitions.
 */

#pragma once

#ifndef __ASM__
#   error "What are you doing?"
#endif

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(_name) \
    .global _name; \
    .type _name, @function; \
    _name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(_name) \
    .type _name, @function; \
    _name:

/** Macro to define the end of a function. */
#define FUNCTION_END(_name) \
    .size _name, . - _name

/** Macro to define a global symbol. */
#define SYMBOL(_name) \
    .global _name; \
    _name:

/** Macro to define a global symbol with alignment. */
#define SYMBOL_ALIGNED(_name, _align) \
    .balign _align; \
    .global _name; \
    _name:

/** Shortcut for ADRP + ADD to load a global symbol address into a register. */
.macro adr_l reg, symbol
    adrp    \reg, \symbol
    add     \reg, \reg, #:lo12:\symbol
.endm
