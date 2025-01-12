/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal window class.
 */

#pragma once

#include "terminal_buffer.h"

#include <device/input.h>

#include <memory>

class Terminal;
class TerminalBuffer;

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

    void handleInput(const input_event_t &event);

    void redraw();
    void bufferUpdated(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void bufferScrolled(uint16_t top, uint16_t bottom, bool up);

private:
    void drawBufferCharacter(TerminalBuffer &buffer, uint16_t x, uint16_t y);
    void drawCharacter(uint16_t x, uint16_t y, uint8_t ch, uint32_t fg, uint32_t bg);

private:
    std::unique_ptr<Terminal> m_terminal;
    uint16_t m_cols;
    uint16_t m_rows;
};
