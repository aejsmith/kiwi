/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Framebuffer device class.
 */

#pragma once

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <kernel/device/kfb.h>

/** Class for drawing to a framebuffer. */
class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();

    uint16_t width() const  { return m_mode.width; }
    uint16_t height() const { return m_mode.height; }

    bool init();

    void putPixel(uint16_t x, uint16_t y, uint32_t rgb);
    void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t rgb);
    void copyRect(
        uint16_t destX, uint16_t destY, uint16_t srcX, uint16_t srcY,
        uint16_t width, uint16_t height);

private:
    void handleRedrawEvent();

private:
    Kiwi::Core::Handle m_handle;

    kfb_mode_t m_mode;
    uint8_t *m_mapping;
    uint8_t *m_backbuffer;
    size_t m_size;

    Kiwi::Core::EventRef m_redrawEvent;
};
