/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Xterm emulator class.
 * 
 * Reference:
 * - Wikipedia: ANSI escape code
 *   http://en.wikipedia.org/wiki/ANSI_escape_code
 * - ASCII Table - ANSI Escape Sequences
 *   http://ascii-table.com/ansi-escape-sequences.php
 * - Xterm Control Sequences
 *   http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 */

#include "terminal_window.h"
#include "xterm.h"

#include <core/log.h>
#include <core/utility.h>

Xterm::Xterm(TerminalWindow &window) :
    Terminal         (window),
    m_mainBuffer     (window, true),
    m_altBuffer      (window, false),
    m_usingAltBuffer (false),
    m_outputFlags    (TerminalBuffer::kOutput_DelayEOL),
    m_escState       (0),
    m_escParamSize   (0),
    m_savedX         (0),
    m_savedY         (0)
{
    m_attributes.ch         = 0;
    m_attributes.fg         = TerminalBuffer::kColour_Default;
    m_attributes.bg         = TerminalBuffer::kColour_Default;
    m_attributes.attributes = 0;
}

Xterm::~Xterm() {}

TerminalBuffer &Xterm::activeBuffer() {
    return (m_usingAltBuffer) ? m_altBuffer : m_mainBuffer;
}

void Xterm::output(uint8_t raw) {
    TerminalBuffer &buffer = activeBuffer();

    int16_t cursorX = buffer.cursorX();
    int16_t cursorY = buffer.cursorY();
    int16_t rows    = m_window.rows();
    int16_t cols    = m_window.cols();

    switch (m_escState) {
        case 0: {
            /* Base state, check for escape characters. */

            if (raw == 0x1b) {
                /* Initially set the parameter size to -1, this allows functions
                 * which have a non-zero default value to see if they need to
                 * use it. */
                m_escParamSize = -1;
                memset(m_escParams, 0, sizeof(m_escParams));
                m_escState = 1;
            } else {
                /* No escape and we're not currently parsing a code, must be a
                 * normal character. */
                TerminalBuffer::Character ch = m_attributes;
                ch.ch = raw;
                buffer.output(ch, m_outputFlags);
            }

            return;
        }
        case 1: {
            /* This state is entered upon an escape character. */

            switch (raw) {
                case '[':
                    /* Control Sequence Indicator (CSI). */
                    m_escState = 2;
                    return;
                case ']':
                    /* Operating System Command (OSC). */
                    m_escState = 6;
                    return;
                case '(':
                case ')':
                    /* Designate G0/1 Character Set. */
                    m_escState = 5;
                    return;
                case '=':
                    /* Application Keypad. TODO. */
                    break;
                case '>':
                    /* Normal Keypad. TODO. */
                    break;
                case '7':
                    /* Save current cursor position. */
                    m_savedX = cursorX;
                    m_savedY = cursorY;
                    break;
                case '8':
                    /* Restore saved cursor position. */
                    buffer.moveCursor(m_savedX, m_savedY);
                    break;
                case 'D':
                    buffer.scrollDown();
                    break;
                case 'M':
                    buffer.scrollUp();
                    break;
                default:
                    core_log(CORE_LOG_WARN, "xterm: unknown character %c at state 1", raw);
                    break;
            }

            break;
        }
        case 2: {
            /* This state handles codes that do not take any arguments. If it
             * encounters a digit or a code not handled by this state it falls
             * through into state 3. */

            bool fall = false;

            switch (raw) {
                case '?':
                    m_escState = 4;
                    return;
                case 's':
                    /* Save current cursor position. */
                    m_savedX = cursorX;
                    m_savedY = cursorY;
                    break;
                case 'u':
                    /* Restore saved cursor position. */
                    buffer.moveCursor(m_savedX, m_savedY);
                    break;
                default:
                    fall = true;
                    break;
            }

            if (!fall)
                break;

            /* Character is a digit or something that takes arguments, fall
             * through into state 3 to process numeric arguments. */
            m_escState = 3;
            [[fallthrough]];
        }
        case 3: {
            /* Collect arguments for the function. Arguments are split with
             * semi-colons. */
            if (isdigit(raw)) {
                if (m_escParamSize < 0)
                    m_escParamSize = 0;

                m_escParams[m_escParamSize] *= 10;
                m_escParams[m_escParamSize] += (raw - '0');
                return;
            } else if (raw == ';') {
                m_escParamSize++;
                if (static_cast<size_t>(m_escParamSize) >= core_array_size(m_escParams)) {
                    core_log(CORE_LOG_WARN, "xterm: exceeded maximum number of escape parameters");
                    break;
                }

                return;
            }

            /* Handle the code. */
            switch (raw) {
                case 'A':
                    /* Cursor Up. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(cursorX, cursorY - m_escParams[0]);
                    break;
                case 'B':
                    /* Cursor Down. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(cursorX, cursorY + m_escParams[0]);
                    break;
                case 'C':
                    /* Cursor Forward. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(cursorX + m_escParams[0], cursorY);
                    break;
                case 'D':
                    /* Cursor Backward. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(cursorX - m_escParams[0], cursorY);
                    break;
                case 'G':
                    /* Cursor Character Absolute. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(m_escParams[0] - 1, cursorY);
                    break;
                case 'H':
                case 'f':
                    /* Move cursor - ESC[{row};{column}H */
                    if (m_escParamSize < 1) {
                        m_escParams[1] = 1;
                        if (m_escParamSize < 0)
                            m_escParams[0] = 1;
                    }

                    buffer.moveCursor(m_escParams[1] - 1, m_escParams[0] - 1);
                    break;
                case 'J':
                    /* Erase in Display. */
                    switch (m_escParams[0]) {
                        case 0:
                            /* Erase Below. */
                            buffer.clearLines(cursorY, rows - 1);
                            break;
                        case 1:
                            /* Erase Above. */
                            buffer.clearLines(0, cursorY);
                            break;
                        case 2:
                            /* Erase All. */
                            buffer.clearLines(0, rows - 1);
                            break;
                    }

                    break;
                case 'K':
                    /* Erase in Line. */
                    switch (m_escParams[0]) {
                        case 0:
                            /* Erase to Right. */
                            buffer.clearLine(cursorX, cols - 1);
                            break;
                        case 1:
                            /* Erase to Left. */
                            buffer.clearLine(0, cursorX);
                            break;
                        case 2:
                            /* Erase All. */
                            buffer.clearLines(cursorY, cursorY);
                            break;
                    }

                    break;
                case 'L':
                    /* Insert Lines (default 1). */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.insertLines(m_escParams[0]);
                    break;
                case 'M':
                    /* Delete Lines (default 1). */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.deleteLines(m_escParams[0]);
                    break;
                case 'P':
                    /* Delete Characters (default 1). */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.deleteChars(m_escParams[0]);
                    break;
                case 'd':
                    /* Line Position Absolute. */
                    if (m_escParamSize < 0)
                        m_escParams[0] = 1;

                    buffer.moveCursor(cursorX, m_escParams[0] - 1);
                    break;
                case 'h':
                    /* Set Mode. */
                    switch (m_escParams[0]) {
                        case 4:
                            /* Insert Mode. */
                            m_outputFlags |= TerminalBuffer::kOutput_Insert;
                            break;
                        default:
                            core_log(CORE_LOG_WARN, "xterm: unhandled mode %d", m_escParams[0]);
                            break;
                    }

                    break;
                case 'l':
                    /* Reset Mode. */
                    switch (m_escParams[0]) {
                        case 4:
                            m_outputFlags &= ~TerminalBuffer::kOutput_Insert;
                            break;
                    }

                    break;
                case 'm':
                    if (m_escParamSize < 0)
                        m_escParamSize = 0;

                    /* Character Attributes (SGR). */
                    for (int i = 0; i <= m_escParamSize; i++) {
                        switch (m_escParams[i]) {
                            case 0:
                                /* Reset attributes to defaults. */
                                m_attributes.fg         = TerminalBuffer::kColour_Default;
                                m_attributes.bg         = TerminalBuffer::kColour_Default;
                                m_attributes.attributes = 0;
                                break;
                            case 1:
                                /* Set bold colour. */
                                m_attributes.attributes |= TerminalBuffer::kAttribute_Bold;
                                break;
                            case 2:
                                /* Set normal colour. */
                                m_attributes.attributes &= ~TerminalBuffer::kAttribute_Bold;
                                break;
                            case 7:
                                /* Inverse colours. */
                                m_attributes.attributes |= TerminalBuffer::kAttribute_Inverse;
                                break;
                            case 27:
                                /* Not inverse colours. */
                                m_attributes.attributes &= ~TerminalBuffer::kAttribute_Inverse;
                                break;
                            case 30 ... 37:
                                /* Set foreground colour. */
                                m_attributes.fg = static_cast<TerminalBuffer::Colour>(m_escParams[i] - 30);
                                break;
                            case 39:
                                /* Reset foreground to default. */
                                m_attributes.fg = TerminalBuffer::kColour_Default;
                                break;
                            case 40 ... 47:
                                /* Set background colour. */
                                m_attributes.bg = static_cast<TerminalBuffer::Colour>(m_escParams[i] - 40);
                                break;
                            case 49:
                                /* Reset background to default. */
                                m_attributes.bg = TerminalBuffer::kColour_Default;
                                break;
                            default:
                                core_log(CORE_LOG_WARN, "xterm: unhandled attribute code %d", m_escParams[i]);
                                break;
                        }
                    }

                    break;
                case 'r':
                    /* Set Scrolling Region. */
                    if (m_escParamSize < 1) {
                        m_escParams[1] = rows;
                        if (m_escParamSize < 0)
                            m_escParams[0] = 1;
                    }

                    buffer.setScrollRegion(m_escParams[0] - 1, m_escParams[1] - 1);
                    break;
                default:
                    core_log(CORE_LOG_WARN, "xterm: unknown character %d%c at state 3", m_escParams[0], raw);
                    break;
            }

            break;
        }
        case 4: {
            /* Handle DEC Private Mode Set/Reset. */
            if (isdigit(raw)) {
                m_escParams[0] *= 10;
                m_escParams[0] += (raw - '0');
                return;
            }

            bool handled = true;

            if (raw == 'h') {
                /* DEC Private Mode Set. */
                switch (m_escParams[0]) {
                    case 1:
                        /* Enable Cursor Keys Application Mode. Ignored for now. */
                        break;
                    case 1049:
                        /* Save Cursor and Use Alternative Screen Buffer. */
                        m_savedX = cursorX;
                        m_savedY = cursorY;
                        [[fallthrough]];
                    case 47:
                    case 1047:
                        /* Use Alternate Screen Buffer. */
                        m_usingAltBuffer = true;
                        m_window.redraw();
                        break;
                    case 1048:
                        /* Save Cursor. */
                        m_savedX = cursorX;
                        m_savedY = cursorY;
                        break;
                    case 2004:
                        /* Bracketed Paste Mode. Ignored for now. */
                        break;
                    default:
                        handled = false;
                        break;
                }
            } else if (raw == 'l') {
                /* DEC Private Mode Reset. */
                switch (m_escParams[0]) {
                    case 1:
                        /* Disable Cursor Keys Application Mode. Ignored for now. */
                        break;
                    case 1049:
                        /* Restore Cursor and Use Normal Screen Buffer. */
                        buffer.moveCursor(m_savedX, m_savedY);
                        [[fallthrough]];
                    case 47:
                    case 1047:
                        /* Use Normal Screen Buffer. */
                        m_usingAltBuffer = false;
                        m_window.redraw();
                        break;
                    case 1048:
                        /* Restore Cursor. */
                        buffer.moveCursor(m_savedX, m_savedY);
                        break;
                    case 2004:
                        /* Bracketed Paste Mode. */
                        break;
                    default:
                        handled = false;
                        break;
                }
            } else {
                handled = false;
            }

            if (!handled)
                core_log(CORE_LOG_WARN, "xterm: unhandled DEC Private Mode %d%c", m_escParams[0], raw);

            break;
        }
        case 5: {
            /* Don't support anything here yet. ESC( and ESC) only have one
             * following character, if we're here we've received it so just
             * ignore it and reset. */
            break;
        }
        case 6: {
            /* Operating System Controls. These take two arguments, an integer
             * and a string. */

            if (m_escParamSize < 0) {
                m_escString.clear();
                m_escParamSize = 0;
            }

            if (m_escParamSize == 0) {
                if (isdigit(raw)) {
                    m_escParams[0] *= 10;
                    m_escParams[0] += (raw - '0');
                    return;
                } else if (raw == ';') {
                    m_escParamSize++;
                    return;
                }
            } else if (raw == 7) {
                /* BEL is end of the command. */
                switch (m_escParams[0]) {
                    case 0:
                    case 2:
                        /* Set Window Title. */
                        //m_window.setTitle(m_escString);
                        break;
                };
            } else {
                if (isprint(raw)) {
                    m_escString += raw;
                    return;
                }
            }

            break;
        }
    }

    /* If we get here then we either completed a sequence or found an invalid
     * code. Both of these cases mean we should reset the state to 0. */
    m_escState = 0;
}
