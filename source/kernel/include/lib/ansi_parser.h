/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               ANSI escape code parser.
 */

#ifndef __LIB_ANSI_PARSER_H
#define __LIB_ANSI_PARSER_H

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

#endif /* __LIB_ANSI_PARSER_H */
