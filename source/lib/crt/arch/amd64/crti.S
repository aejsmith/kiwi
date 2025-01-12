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
_init:
    subq    $8, %rsp

.section .fini
.global _fini
.type _fini, @function
_fini:
    subq    $8, %rsp

.section ".note.GNU-stack", "", @progbits
