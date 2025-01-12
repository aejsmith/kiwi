/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal buffer class.
 */

#include "terminal_buffer.h"
#include "terminal_window.h"

#include <algorithm>

#include <assert.h>
#include <stdlib.h>

static const TerminalBuffer::Character kEmptyCharacter = {
    ' ',
    TerminalBuffer::kColour_Default,
    TerminalBuffer::kColour_Default,
    0
};

TerminalBuffer::TerminalBuffer(TerminalWindow &window, bool useHistory) :
    m_window        (window),
    m_useHistory    (useHistory),
    m_cursorX       (0),
    m_cursorY       (0),
    m_pendingEOL    (false),
    m_scrollTop     (0),
    m_scrollBottom  (m_window.rows() - 1)
{
    for (uint16_t i = 0; i < m_window.rows(); i++)
        m_lines.emplace_back(std::make_unique<Line>());
}

TerminalBuffer::~TerminalBuffer() {}

/** Get a character from the buffer.
 * @param x             X position to get from.
 * @param y             Y position to get from (if negative, will get from
 *                      history).
 * @return              Character read. */
TerminalBuffer::Character TerminalBuffer::charAt(uint16_t x, int16_t y) {
    size_t pos = abs(y);

    Line *line;

    if (y < 0) {
        assert(pos <= m_history.size());
        line = m_history[m_history.size() - pos].get();
    } else {
        assert(pos < m_lines.size());
        line = m_lines[pos].get();
    }

    return (x < line->size()) ? (*line)[x] : kEmptyCharacter;
}

void TerminalBuffer::setChar(uint16_t x, uint16_t y, Character ch) {
    Line &line = *m_lines[y];

    if (x >= line.size())
        line.resize(x + 1, kEmptyCharacter);

    line[x] = ch;
}

/** Insert lines at the current cursor position. */
void TerminalBuffer::insertLines(uint16_t count) {
    /* Only affects the scroll region. If the cursor is outside it, ignore it. */
    if (m_cursorY < m_scrollTop || m_cursorY > m_scrollBottom)
        return;

    for (uint16_t i = 0; i < count; i++) {
        /* Erase from the bottom, insert at current position. */
        m_lines.erase(m_lines.begin() + m_scrollBottom);
        m_lines.insert(m_lines.begin() + m_cursorY, std::make_unique<Line>());
    }

    /* Have to update from the current line to the rest of the scroll region. */
    // TODO: We could use bufferScrolled if count == 1 which might be faster.
    m_window.bufferUpdated(0, m_cursorY, m_window.cols(), (m_scrollBottom - m_cursorY) + 1);
}

/** Delete lines at the current cursor position. */
void TerminalBuffer::deleteLines(uint16_t count) {
    /* Only affects the scroll region. If the cursor is outside it, ignore it. */
    if (m_cursorY < m_scrollTop || m_cursorY > m_scrollBottom)
        return;

    for (uint16_t i = 0; i < count; i++) {
        /* Erase from the current position, insert at the bottom. */
        m_lines.erase(m_lines.begin() + m_cursorY);
        m_lines.insert(m_lines.begin() + m_scrollBottom, std::make_unique<Line>());
    }

    /* Have to update from the current line to the rest of the scroll region. */
    // TODO: We could use bufferScrolled if count == 1 which might be faster.
    m_window.bufferUpdated(0, m_cursorY, m_window.cols(), (m_scrollBottom - m_cursorY) + 1);
}

/** Clear part of the current line (inclusive). */
void TerminalBuffer::clearLine(uint16_t startX, uint16_t endX) {
    assert(startX <= endX);
    assert(endX < m_window.cols());

    for (uint16_t i = startX; i <= endX; i++)
        setChar(i, m_cursorY, kEmptyCharacter);

    m_window.bufferUpdated(startX, m_cursorY, (endX - startX) + 1, 1);
}

/** Clear lines on the buffer (inclusive). */
void TerminalBuffer::clearLines(uint16_t startY, uint16_t endY) {
    assert(startY <= endY);
    assert(endY < m_window.rows());

    for (uint16_t i = startY; i <= endY; i++)
        m_lines[i]->clear();

    m_window.bufferUpdated(0, startY, m_window.cols(), (endY - startY) + 1);
}

/** Insert spaces right of the current position. */
void TerminalBuffer::insertChars(uint16_t count) {
    Line &line = *m_lines[m_cursorY];

    if (m_cursorX >= line.size())
        return;

    line.insert(line.begin() + m_cursorX, count, kEmptyCharacter);

    while (line.size() > m_window.cols())
        line.pop_back();

    m_window.bufferUpdated(m_cursorX, m_cursorY, line.size() - m_cursorX, 1);
}

/** Delete characters right of the current position and shift in spaces. */
void TerminalBuffer::deleteChars(uint16_t count) {
    Line &line = *m_lines[m_cursorY];

    uint16_t start = std::min(line.size(), static_cast<size_t>(m_cursorX));
    uint16_t end   = std::min(line.size(), static_cast<size_t>(m_cursorX + count));

    line.erase(line.begin() + start, line.begin() + end);

    m_window.bufferUpdated(m_cursorX, m_cursorY, m_window.cols() - m_cursorX, 1);
}

/** Scroll the buffer up (move contents down). */
void TerminalBuffer::scrollUp() {
    if (m_scrollTop == 0 && m_scrollBottom == (m_window.rows() - 1)) {
        m_lines.pop_back();
        m_lines.emplace_front(std::make_unique<Line>());
    } else {
        /* Remove a line from the end of the scroll region, then add a new line
         * at the top of it. */
        m_lines.erase(m_lines.begin() + m_scrollBottom);
        m_lines.insert(m_lines.begin() + m_scrollTop, std::make_unique<Line>());
    }

    m_window.bufferScrolled(m_scrollTop, m_scrollBottom, true);
}

/** Scroll the buffer down (move contents up). */
void TerminalBuffer::scrollDown() {
    if (m_scrollTop == 0 && m_scrollBottom == (m_window.rows() - 1)) {
        /* Scroll region is equal to entire visible area: we can push the top
         * line back to history. */
        if (m_useHistory) {
            m_history.emplace_back(std::move(m_lines[0]));
            if (m_history.size() > kMaxHistorySize)
                m_history.pop_front();
        }

        m_lines.pop_front();
        m_lines.emplace_back(std::make_unique<Line>());
    } else {
        /* Insert a new line at the end of the scroll region, then remove the
         * line at the top of it. */
        m_lines.insert(m_lines.begin() + (m_scrollBottom + 1), std::make_unique<Line>());
        m_lines.erase(m_lines.begin() + m_scrollTop);
    }

    m_window.bufferScrolled(m_scrollTop, m_scrollBottom, false);
}

/** Set the scroll region (inclusive). */
void TerminalBuffer::setScrollRegion(int16_t top, int16_t bottom) {
    uint16_t rows = m_window.rows();

    /* Check the values (from user input). */
    if (top >= bottom || top < 0 || top >= rows || bottom < 0 || bottom >= rows) {
        top    = 0;
        bottom = rows - 1;
    }

    m_scrollTop    = top;
    m_scrollBottom = bottom;
}

void TerminalBuffer::moveCursor(int16_t x, int16_t y) {
    uint16_t cols = m_window.cols();
    uint16_t rows = m_window.rows();

    /* Check for valid values (from user input). */
    if (x < 0)     { x = 0; }
    if (x >= cols) { x = cols - 1; }
    if (y < 0)     { y = 0; }
    if (y >= rows) { y = rows - 1; }

    uint16_t prevX = m_cursorX;
    uint16_t prevY = m_cursorY;
    m_cursorX      = x;
    m_cursorY      = y;
    m_pendingEOL   = false;

    m_window.bufferUpdated(prevX, prevY, 1, 1);
    m_window.bufferUpdated(m_cursorX, m_cursorY, 1, 1);
}

void TerminalBuffer::output(Character ch, uint32_t flags) {
    uint16_t cols = m_window.cols();

    uint16_t prevX = m_cursorX;
    uint16_t prevY = m_cursorY;

    switch (ch.ch) {
        case '\b':
            /* Backspace, move back one character if we can. */
            if (m_cursorX) {
                m_cursorX--;
            } else if (m_cursorY) {
                m_cursorX = cols - 1;
                m_cursorY--;
            }

            break;
        case '\r':
            /* Carriage return, move to the start of the line. */
            m_cursorX = 0;
            break;
        case '\n':
            /* Newline, treat it as if a carriage return was also there. */
            m_cursorX = 0;
            m_cursorY++;
            break;
        case '\t':
            m_cursorX += 8 - (m_cursorX % 8);
            break;
        default:
            /* If it is a non-printing character, ignore it. */
            if (ch.ch >= ' ') {
                if (m_pendingEOL) {
                    m_pendingEOL = false;

                    /* Run through the whole output again to do this as we
                     * might need to scroll before we can write the new char. */
                    Character eol = ch;
                    eol.ch = '\n';
                    output(eol, flags);

                    /* Cursor will have changed. */
                    prevX = m_cursorX;
                    prevY = m_cursorY;
                }

                if (flags & kOutput_Insert)
                    insertChars(1);

                setChar(m_cursorX, m_cursorY, ch);
                m_cursorX++;
            }

            break;
    }

    /* If we have reached the edge of the buffer move to a new line. */
    if (m_cursorX >= cols) {
        if (flags & kOutput_DelayEOL) {
            /* Xterm delayed EOL behaviour - newline is deferred to the next
             * printable character. An explicit newline in between clears this
             * so that you only get one newline. */
            m_cursorX    = cols - 1;
            m_pendingEOL = true;
        } else {
            m_cursorX = 0;
            m_cursorY++;
        }
    } else {
        m_pendingEOL = false;
    }

    /* Redraw our previous position to display any newly written character and
     * clear the cursor. */
    bool isDirty = m_cursorX != prevX || m_cursorY != prevY;
    if (isDirty)
        m_window.bufferUpdated(prevX, prevY, 1, 1);

    /* If we have reached the bottom of the scroll region, scroll. */
    if (m_cursorY > m_scrollBottom) {
        m_cursorY = m_scrollBottom;
        scrollDown();
    }

    if (isDirty)
        m_window.bufferUpdated(m_cursorX, m_cursorY, 1, 1);
}
