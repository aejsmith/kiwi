/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 application startup code.
 */

.section .text

/** Main program entry point. */
.global _start
.type _start, @function
_start:
    jmp     libsystem_main
.size _start, .-_start

.section ".note.GNU-stack", "", @progbits
