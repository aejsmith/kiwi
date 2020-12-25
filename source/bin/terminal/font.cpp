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
 * @brief               Font class.
 */

#include "font.h"

#include <core/log.h>

#include <algorithm>

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

static constexpr FT_UInt kFontDpi = 96;

/** Get the singleton FT_Library instance. */
static FT_Library freeTypeLibrary() {
    static FT_Library s_library = [] () {
        FT_Library library;
        FT_Init_FreeType(&library);
        return library;
    }();

    return s_library;
}

Font::Font() :
    m_face   (nullptr),
    m_width  (0),
    m_height (0)
{}

Font::~Font() {
    if (m_face) {
        FT_Face face = reinterpret_cast<FT_Face>(m_face);
        FT_Done_Face(face);
    }
}

bool Font::init(const char *path, uint32_t pointSize) {
    static const FT_Int32 kLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT;

    // TODO: Unicode

    assert(!m_face);

    FT_Library library = freeTypeLibrary();

    FT_Face face;
    FT_Error ret = FT_New_Face(library, path, 0, &face);
    if (ret) {
        core_log(CORE_LOG_ERROR, "failed to load font: %d", ret);
        return false;
    }

    m_face = face;

    if (!FT_IS_FIXED_WIDTH(face)) {
        core_log(CORE_LOG_ERROR, "font is not fixed width");
        return false;
    }

    FT_Set_Char_Size(face, 0, pointSize * 64, kFontDpi, kFontDpi);

    /* Determine maximum font height. Divide by 64 to get pixels. */
    FT_Pos maxAscender  = face->size->metrics.ascender / 64;
    FT_Pos maxDescender = -(face->size->metrics.descender / 64);
    m_height            = std::max(maxAscender + maxDescender, face->size->metrics.height / 64);

    /* Determine the maximum width. For this we have to check each glyph in
     * turn. */
    m_width = 0;
    for (FT_ULong ch = 0; ch < 256; ch++) {
        if (FT_Load_Char(face, ch, kLoadFlags) != 0) {
            /* If loading a glyph fails, load a question mark in its place. */
            if (FT_Load_Char(face, '?', kLoadFlags) != 0) {
                core_log(CORE_LOG_ERROR, "loading font glyph %lu failed", ch);
                return false;
            }
        }

        m_width = std::max(m_width, static_cast<uint16_t>(face->glyph->advance.x / 64));
    }

    /* Pre-render font glyphs. */
    size_t dataSize = 256 * m_width * m_height;
    m_data = std::make_unique<uint8_t[]>(dataSize);
    memset(m_data.get(), 0, dataSize);

    for (FT_ULong ch = 0; ch < 256; ch++) {
        if (FT_Load_Char(face, ch, kLoadFlags) != 0) {
            /* If loading a glyph fails, load a question mark in its place. */
            if (FT_Load_Char(face, '?', kLoadFlags) != 0) {
                core_log(CORE_LOG_ERROR, "loading font glyph %lu failed", ch);
                return false;
            }
        }

        uint32_t x      = face->glyph->bitmap_left;
        uint32_t y      = maxAscender - face->glyph->bitmap_top;
        uint32_t width  = std::min(face->glyph->bitmap.width, m_width - x);
        uint32_t height = std::min(face->glyph->bitmap.rows, m_height - y);

        /* Load the texture data. FIXME: Correctly handle bitmap formats. */
        uint8_t *data   = &m_data[ch * m_height * m_width];
        uint8_t *bitmap = face->glyph->bitmap.buffer;
        for (uint32_t offsetX = 0; offsetX < width; offsetX++) {
            for (uint32_t offsetY = 0; offsetY < height; offsetY++) {
                data[((y + offsetY) * m_width) + x + offsetX] =
                    bitmap[(offsetY * face->glyph->bitmap.width) + offsetX];
            }
        }
    }

    return true;
}
