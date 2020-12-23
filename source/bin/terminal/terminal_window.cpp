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

#include "framebuffer.h"
#include "terminal_app.h"
#include "terminal_window.h"
#include "xterm.h"

#include "../../kernel/console/font.c"

static constexpr uint16_t kFontWidth  = 7;
static constexpr uint16_t kFontHeight = 14;

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
    m_cols (g_terminalApp.framebuffer().width() / kFontWidth),
    m_rows (g_terminalApp.framebuffer().height() / kFontHeight)
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

            if (ch.fg == TerminalBuffer::kColour_Default)
                ch.fg = kDefaultForegroundColour;
            if (ch.bg == TerminalBuffer::kColour_Default)
                ch.bg = kDefaultBackgroundColour;

            /* Swap colours for the cursor. */
            if (currX == buffer.cursorX() && currY == buffer.cursorY())
                std::swap(ch.fg, ch.bg);

            drawCharacter(currX, currY, ch);
        }
    }
}

void TerminalWindow::bufferScrolled(uint16_t top, uint16_t bottom, bool up) {
    Framebuffer &fb = g_terminalApp.framebuffer();

    uint16_t y      = top * kFontHeight;
    uint16_t height = (bottom - top) * kFontHeight;

    if (up) {
        /* Scroll up - move the contents down. */
        fb.copyRect(0, y + kFontHeight, 0, y, fb.width(), height);
        fb.fillRect(0, y, fb.width(), kFontHeight, 0);
    } else {
        /* Scroll down - move the contents up. */
        fb.copyRect(0, y, 0, y + kFontHeight, fb.width(), height);
        fb.fillRect(0, y + height, fb.width(), kFontHeight, 0);
    }
}

void TerminalWindow::drawCharacter(uint16_t x, uint16_t y, TerminalBuffer::Character ch) {
    const uint32_t *colours = (ch.attributes & TerminalBuffer::kAttribute_Bold)
        ? kColourTableBold
        : kColourTable;

    Framebuffer &fb = g_terminalApp.framebuffer();

    x *= kFontWidth;
    y *= kFontHeight;

    for (uint16_t i = 0; i < kFontHeight; i++) {
        for (uint16_t j = 0; j < kFontWidth; j++) {
            if (console_font[(ch.ch * kFontHeight) + i] & (1 << (7 - j))) {
                fb.putPixel(x + j, y + i, colours[ch.fg]);
            } else {
                fb.putPixel(x + j, y + i, colours[ch.bg]);
            }
        }
    }
}
