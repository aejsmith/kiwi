/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 assembly code definitions.
 */

#pragma once

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(name) \
    .global name; \
    .type name, @function; \
    name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(name) \
    .type name, @function; \
    name:

/** Macro to define the end of a function. */
#define FUNCTION_END(name) \
    .size name, . - name

/** Macro to define a global symbol. */
#define SYMBOL(name) \
    .global name; \
    name: