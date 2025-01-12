/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Font class.
 */

#pragma once

#include <memory>

#include <stdint.h>

class Font {
public:
    Font();
    ~Font();

    bool init(const char *path, uint32_t pointSize);

    uint16_t width() const  { return m_width; }
    uint16_t height() const { return m_height; }

    const uint8_t *charData(uint8_t ch) const;

private:
    void *m_face;
    uint16_t m_width;
    uint16_t m_height;
    std::unique_ptr<uint8_t[]> m_data;
};

inline const uint8_t *Font::charData(uint8_t ch) const {
    return &m_data[ch * m_height * m_width];
}
