/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

    /** Current character/output attributes. */
    TerminalBuffer::Character m_attributes;
    uint32_t m_outputFlags;

    /** Escape code state. */
    int m_escState;
    int m_escParams[8];
    int m_escParamSize;
    std::string m_escString;

    /** Saved cursor position. */
    uint16_t m_savedX;
    uint16_t m_savedY;
};
