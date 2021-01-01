/*
 * Copyright (C) 2009-2021 Alex Smith
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

#pragma once

#include "terminal_buffer.h"
#include "terminal.h"

/** Implementation of an Xterm-compatible terminal. */
class Xterm final : public Terminal {
public:
    Xterm(TerminalWindow &window);
    ~Xterm();

    TerminalBuffer &activeBuffer() override;
    void output(uint8_t raw) override;

private:
    /** Main and alternate buffers. */
    TerminalBuffer m_mainBuffer;
    TerminalBuffer m_altBuffer;
    bool m_usingAltBuffer;

    /** Current character attributes. */
    TerminalBuffer::Character m_attributes;

    /** Escape code state. */
    int m_escState;
    int m_escParams[8];
    int m_escParamSize;
    std::string m_escString;

    /** Saved cursor position. */
    uint16_t m_savedX;
    uint16_t m_savedY;
};
