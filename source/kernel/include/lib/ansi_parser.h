/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ANSI escape code parser.
 */

#pragma once

#include <types.h>

/** Buffer length for ANSI escape code parser. */
#define ANSI_PARSER_BUFFER_LEN  3

/** ANSI escape code parser structure. */
typedef struct ansi_parser {
    /** Buffer containing collected sequence. */
    char buffer[ANSI_PARSER_BUFFER_LEN];

    int length;                     /**< Buffer length. */
} ansi_parser_t;

extern uint16_t ansi_parser_filter(ansi_parser_t *parser, unsigned char ch);
extern void ansi_parser_init(ansi_parser_t *parser);
