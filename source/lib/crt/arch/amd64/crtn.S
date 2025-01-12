/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 C runtime.
 */

.section .init
.global _init
.type _init, @function
    addq    $8, %rsp
    ret

.section .fini
.global _fini
.type _fini, @function
    addq    $8, %rsp
    ret

.section ".note.GNU-stack", "", @progbits
