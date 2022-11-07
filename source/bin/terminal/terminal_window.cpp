/*
 * Copyright (C) 2009-2022 Alex Smith
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

#include "font.h"
#include "framebuffer.h"
#include "terminal_app.h"
#include "terminal_window.h"
#include "xterm.h"

/* Tango colour scheme. */

static constexpr TerminalBuffer::Colour kDefaultBackgroundColour = TerminalBuffer::kColour_Black;
static constexpr TerminalBuffer::Colour kDefaultForegroundColour = TerminalBuffer::kColour_White;

static constexpr uint32_t kColourTableBold[8] = {
    0x555753,
    0xef2929,
    0x8ae234,
    0xfce94f,
    0x729fcf,
    0xad7fa8,
    0x34e2e2,
    0xeeeeec
};

static constexpr uint32_t kColourTable[8] = {
    0x000000,
    0xcc0000,
    0x4e9a06,
    0xc4a000,
    0x3465a4,
    0x75507b,
    0x06989a,
    0xd3d7cf
};

TerminalWindow::TerminalWindow() :
    m_cols (g_terminalApp.framebuffer().width() / g_terminalApp.font().width()),
    m_rows (g_terminalApp.framebuffer().height() / g_terminalApp.font().height())
{}

TerminalWindow::~TerminalWindow() {
    g_terminalApp.removeWindow(this);
}

bool TerminalWindow::init() {
    m_terminal = std::make_unique<Xterm>(*this);
    return m_terminal->init();
}

void TerminalWindow::close() {
    delete this;
}

void TerminalWindow::handleInput(const input_event_t &event) {
    Keyboard &keyboard = g_terminalApp.keyboard();

    if (event.type == INPUT_EVENT_KEY_DOWN && keyboard.modifiers() == 0) {
        switch (event.value) {
            case INPUT_KEY_INSERT:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[2~");
                return;
            case INPUT_KEY_HOME:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[H");
                return;
            case INPUT_KEY_PAGE_UP:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[5~");
                return;
            case INPUT_KEY_PAGE_DOWN:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[6~");
                return;
            case INPUT_KEY_END:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[F");
                return;
            case INPUT_KEY_DELETE:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[3~");
                return;
            case INPUT_KEY_UP:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[A");
                return;
            case INPUT_KEY_DOWN:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[B");
                return;
            case INPUT_KEY_LEFT:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[D");
                return;
            case INPUT_KEY_RIGHT:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[C");
                return;
            case INPUT_KEY_F1:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("OP");
                return;
            case INPUT_KEY_F2:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("OQ");
                return;
            case INPUT_KEY_F3:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("OR");
                return;
            case INPUT_KEY_F4:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("OS");
                return;
            case INPUT_KEY_F5:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[15~");
                return;
            case INPUT_KEY_F6:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[17~");
                return;
            case INPUT_KEY_F7:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[18~");
                return;
            case INPUT_KEY_F8:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[19~");
                return;
            case INPUT_KEY_F9:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[20~");
                return;
            case INPUT_KEY_F10:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[21~");
                return;
            case INPUT_KEY_F11:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[23~");
                return;
            case INPUT_KEY_F12:
                m_terminal->sendInput(0x1b);
                m_terminal->sendInput("[24~");
                return;
        }
    }

    uint8_t buf[4];
    size_t len = keyboard.map(event, buf);
    m_terminal->sendInput(buf, len);
}

void TerminalWindow::redraw() {
    // TODO: When history scrollback support is added this needs to handle our
    // current offset.
    bufferUpdated(0, 0, m_cols, m_rows);
}

void TerminalWindow::bufferUpdated(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (this != &g_terminalApp.activeWindow())
        return;

    TerminalBuffer &buffer = m_terminal->activeBuffer();

    for (uint16_t offY = 0; offY < height; offY++) {
        uint16_t currY = y + offY;

        for (uint16_t offX = 0; offX < width; offX++) {
            uint16_t currX = x + offX;

            TerminalBuffer::Character ch = buffer.charAt(currX, currY);

            uint32_t fg = kColourTable[kDefaultForegroundColour];
            uint32_t bg = kColourTable[kDefaultBackgroundColour];

            const uint32_t *colours = (ch.attributes & TerminalBuffer::kAttribute_Bold)
                ? kColourTableBold
                : kColourTable;

            if (ch.fg != TerminalBuffer::kColour_Default)
                fg = colours[ch.fg];
            if (ch.bg != TerminalBuffer::kColour_Default)
                bg = colours[ch.bg];

            /* Handle inversed colours. */
            if (ch.attributes & TerminalBuffer::kAttribute_Inverse)
                std::swap(fg, bg);

            /* Swap colours for the cursor. */
            if (currX == buffer.cursorX() && currY == buffer.cursorY())
                std::swap(fg, bg);

            drawCharacter(currX, currY, ch.ch, fg, bg);
        }
    }
}

void TerminalWindow::bufferScrolled(uint16_t top, uint16_t bottom, bool up) {
    Framebuffer &fb = g_terminalApp.framebuffer();
    Font &font      = g_terminalApp.font();

    uint16_t y      = top * font.height();
    uint16_t height = (bottom - top) * font.height();

    if (up) {
        /* Scroll up - move the contents down. */
        fb.copyRect(0, y + font.height(), 0, y, fb.width(), height);
        fb.fillRect(0, y, fb.width(), font.height(), 0);
    } else {
        /* Scroll down - move the contents up. */
        fb.copyRect(0, y, 0, y + font.height(), fb.width(), height);
        fb.fillRect(0, y + height, fb.width(), font.height(), 0);
    }
}

static inline uint32_t blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    auto r = [] (uint32_t v) { return ((v >> 16) & 0xff); };
    auto g = [] (uint32_t v) { return ((v >> 8) & 0xff); };
    auto b = [] (uint32_t v) { return (v & 0xff); };

    uint32_t a = alpha + 1;
    uint32_t inv_a = 256 - alpha;

    uint32_t result;
    result  = (((a * r(fg)) + (inv_a * r(bg))) & 0x00ff00) << 8;
    result |= (((a * g(fg)) + (inv_a * g(bg))) & 0x00ff00);
    result |= (((a * b(fg)) + (inv_a * b(bg))) & 0x00ff00) >> 8;
    return result;
}

void TerminalWindow::drawCharacter(uint16_t x, uint16_t y, uint8_t ch, uint32_t fg, uint32_t bg) {
    Framebuffer &fb = g_terminalApp.framebuffer();
    Font &font      = g_terminalApp.font();

    x *= font.width();
    y *= font.height();

    const uint8_t *data = font.charData(ch);

    for (uint16_t i = 0; i < font.height(); i++) {
        for (uint16_t j = 0; j < font.width(); j++) {
            uint32_t result = blend(fg, bg, *data);
            ++data;
            fb.putPixel(x + j, y + i, result);
        }
    }
}
