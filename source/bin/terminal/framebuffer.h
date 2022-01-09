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
 * @brief               Framebuffer device class.
 */

#pragma once

#include "event_handler.h"

#include <kernel/device/kfb.h>

/** Class for drawing to a framebuffer. */
class Framebuffer : public EventHandler {
public:
    Framebuffer();
    ~Framebuffer();

    uint16_t width() const  { return m_mode.width; }
    uint16_t height() const { return m_mode.height; }

    bool init();

    void handleEvent(const object_event_t &event) override;

    void putPixel(uint16_t x, uint16_t y, uint32_t rgb);
    void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t rgb);
    void copyRect(
        uint16_t destX, uint16_t destY, uint16_t srcX, uint16_t srcY,
        uint16_t width, uint16_t height);

private:
    handle_t m_handle;
    kfb_mode_t m_mode;
    uint8_t *m_mapping;
    uint8_t *m_backbuffer;
    size_t m_size;
};
