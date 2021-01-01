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
        /* Insert a new line at the end of the scroll region, then
         * remove the line at the top of it. */
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
    m_cursorX = x;
    m_cursorY = y;

    m_window.bufferUpdated(prevX, prevY, 1, 1);
    m_window.bufferUpdated(m_cursorX, m_cursorY, 1, 1);
}

void TerminalBuffer::output(Character ch) {
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
                setChar(m_cursorX, m_cursorY, ch);
                m_cursorX++;
            }

            break;
    }

    /* If we have reached the edge of the console insert a new line. */
    if (m_cursorX >= cols) {
        m_cursorX = 0;
        m_cursorY++;
    }

    if (m_cursorX != prevX || m_cursorY != prevY)
        m_window.bufferUpdated(prevX, prevY, 1, 1);

    /* If we have reached the bottom of the scroll region, scroll. */
    if (m_cursorY > m_scrollBottom) {
        m_cursorY = m_scrollBottom;
        scrollDown();
    }

    if (m_cursorX != prevX || m_cursorY != prevY)
        m_window.bufferUpdated(m_cursorX, m_cursorY, 1, 1);
}
