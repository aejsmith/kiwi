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
 * @brief               Xterm emulator class.
 */

#include "xterm.h"

Xterm::Xterm(TerminalWindow &window) :
    Terminal (window),
    m_buffer (window, true)
{
    m_attributes.ch         = 0;
    m_attributes.fg         = TerminalBuffer::kColour_Default;
    m_attributes.bg         = TerminalBuffer::kColour_Default;
    m_attributes.attributes = 0;
}

Xterm::~Xterm() {}

TerminalBuffer &Xterm::activeBuffer() {
    return m_buffer;
}

void Xterm::output(uint8_t raw) {
    // TODO: Xterm

    TerminalBuffer::Character ch = m_attributes;
    ch.ch = raw;
    m_buffer.output(ch);
}
