/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 application startup code.
 */

.section .text

/** Main program entry point. */
.global _start
.type _start, @function
_start:
    b       libsystem_main
.size _start, .-_start

.section ".note.GNU-stack", "", @progbits
