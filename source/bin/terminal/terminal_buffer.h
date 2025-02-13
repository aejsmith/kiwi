/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal buffer class.
 */

#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <stdint.h>

class TerminalWindow;

/** Class implementing a terminal data buffer. */
class TerminalBuffer {
public:
    static constexpr size_t kMaxHistorySize = 2048;

    enum Colour : uint8_t {
        /** Specific colours, values correspond to ANSI escape codes. */
        kColour_Black   = 0,
        kColour_Red     = 1,
        kColour_Green   = 2,
        kColour_Yellow  = 3,
        kColour_Blue    = 4,
        kColour_Magenta = 5,
        kColour_Cyan    = 6,
        kColour_White   = 7,

        /** No specific colour, use default. */
        kColour_Default
    };

    enum Attribute : uint8_t {
        kAttribute_Bold     = (1<<0),
        kAttribute_Inverse  = (1<<1),
    };

    /** Output behaviour flags. */
    enum Output {
        /** Insert rather than overwriting characters. */
        kOutput_Insert = (1<<0),

        /** Xterm delayed EOL behaviour. */
        kOutput_DelayEOL = (1<<1),
    };

    struct Character {
        // TODO: Unicode.
        uint8_t ch;
        Colour fg;
        Colour bg;
        uint8_t attributes;
    };

public:
    TerminalBuffer(TerminalWindow &window, bool useHistory);
    ~TerminalBuffer();

    size_t historySize() const  { return m_history.size(); }
    uint16_t cursorX() const    { return m_cursorX; }
    uint16_t cursorY() const    { return m_cursorY; }

    Character charAt(uint16_t x, int16_t y);

    void insertLines(uint16_t count);
    void deleteLines(uint16_t count);
    void clearLine(uint16_t startX, uint16_t endX);
    void clearLines(uint16_t startY, uint16_t endY);
    void insertChars(uint16_t count);
    void deleteChars(uint16_t count);
    void scrollUp();
    void scrollDown();
    void setScrollRegion(int16_t top, int16_t bottom);
    void moveCursor(int16_t x, int16_t y);
    void output(Character ch, uint32_t flags = 0);

private:
    using Line      = std::vector<Character>;
    using LineDeque = std::deque<std::unique_ptr<Line>>;

    void setChar(uint16_t x, uint16_t y, Character ch);

private:
    TerminalWindow &m_window;           /**< Window displaying the buffer. */
    const bool m_useHistory;            /**< Whether keeping history. */

    LineDeque m_lines;                  /**< Lines in the main area. */
    LineDeque m_history;                /**< Lines in the history. */
    uint16_t m_cursorX;                 /**< X position of cursor. */
    uint16_t m_cursorY;                 /**< Y position of cursor. */
    bool m_pendingEOL;                  /**< Delayed EOL is pending. */
    uint16_t m_scrollTop;               /**< Top of scroll region. */
    uint16_t m_scrollBottom;            /**< Bottom of scroll region. */
};
