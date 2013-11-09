/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		ANSI escape code parser.
 */

#include <lib/ansi_parser.h>
#include <lib/string.h>

#include <console.h>

/** Handle an input character.
 * @param parser	Parser data to use.
 * @param ch		Character received.
 * @return		Value to return, 0 if no character to return yet. */
uint16_t ansi_parser_filter(ansi_parser_t *parser, unsigned char ch) {
	uint16_t ret = 0;

	if(parser->length < 0) {
		if(ch == 0x1B) {
			parser->length = 0;
			return 0;
		} else {
			return (uint16_t)ch;
		}
	} else {
		parser->buffer[parser->length++] = ch;

		/* Check for known sequences. */
		if(parser->length == 2) {
			if(strncmp(parser->buffer, "[A", 2) == 0) {
				ret = CONSOLE_KEY_UP;
			} else if(strncmp(parser->buffer, "[B", 2) == 0) {
				ret = CONSOLE_KEY_DOWN;
			} else if(strncmp(parser->buffer, "[D", 2) == 0) {
				ret = CONSOLE_KEY_LEFT;
			} else if(strncmp(parser->buffer, "[C", 2) == 0) {
				ret = CONSOLE_KEY_RIGHT;
			} else if(strncmp(parser->buffer, "[H", 2) == 0) {
				ret = CONSOLE_KEY_HOME;
			} else if(strncmp(parser->buffer, "[F", 2) == 0) {
				ret = CONSOLE_KEY_END;
			}
		} else if(parser->length == 3) {
			if(strncmp(parser->buffer, "[3~", 3) == 0) {
				ret = 0x7F;
			} else if(strncmp(parser->buffer, "[5~", 3) == 0) {
				ret = CONSOLE_KEY_PGUP;
			} else if(strncmp(parser->buffer, "[6~", 3) == 0) {
				ret = CONSOLE_KEY_PGDN;
			}
		}

		if(ret != 0 || parser->length == ANSI_PARSER_BUFFER_LEN)
			parser->length = -1;

		return ret;
	}
}

/** Initialize an ANSI escape code parser data structure.
 * @param parser	Parser to initialize. */
void ansi_parser_init(ansi_parser_t *parser) {
	parser->length = -1;
}
