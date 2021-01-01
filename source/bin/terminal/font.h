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
