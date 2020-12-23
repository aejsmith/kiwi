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
 * @brief               Terminal window class.
 */

#pragma once

#include "terminal_buffer.h"

#include <memory>

class Terminal;

/**
 * Class implementing a terminal window. This handles rendering the contents of
 * a terminal, currently to the framebuffer.
 */
class TerminalWindow {
public:
    TerminalWindow();
    ~TerminalWindow();

    Terminal &terminal() const      { return *m_terminal; }
    uint16_t cols() const           { return m_cols; }
    uint16_t rows() const           { return m_rows; }

    bool init();
    void close();

    void redraw();
    void bufferUpdated(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void bufferScrolled(uint16_t top, uint16_t bottom, bool up);

private:
    void drawCharacter(uint16_t x, uint16_t y, TerminalBuffer::Character ch);

    std::unique_ptr<Terminal> m_terminal;
    uint16_t m_cols;
    uint16_t m_rows;
};
