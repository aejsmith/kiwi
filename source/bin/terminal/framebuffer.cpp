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

#include "framebuffer.h"
#include "terminal_app.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/status.h>
#include <kernel/system.h>
#include <kernel/vm.h>

#include <inttypes.h>

static constexpr char kKfbDevicePath[] = "/virtual/kfb";

Framebuffer::Framebuffer() :
    m_handle     (INVALID_HANDLE),
    m_mapping    (nullptr),
    m_backbuffer (nullptr)
{}

Framebuffer::~Framebuffer() {
    g_terminalApp.removeEvents(this);

    if (m_backbuffer)
        kern_vm_unmap(m_backbuffer, m_size);

    if (m_mapping)
        kern_vm_unmap(m_mapping, m_size);

    if (m_handle != INVALID_HANDLE)
        kern_handle_close(m_handle);
}

bool Framebuffer::init() {
    status_t ret;

    ret = kern_device_open(kKfbDevicePath, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &m_handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open device: %" PRId32, ret);
        return false;
    }

    ret = kern_file_request(m_handle, KFB_DEVICE_REQUEST_MODE, nullptr, 0, &m_mode, sizeof(m_mode), nullptr);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to get mode: %" PRId32, ret);
        return false;
    }

    ret = kern_file_request(m_handle, KFB_DEVICE_REQUEST_ACQUIRE, nullptr, 0, nullptr, 0, nullptr);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to acquire framebuffer: %" PRId32, ret);
        return false;
    }

    size_t pageSize;
    kern_system_info(SYSTEM_INFO_PAGE_SIZE, &pageSize);

    m_size = core_round_up(m_mode.pitch * m_mode.height, pageSize);

    ret = kern_vm_map(
        reinterpret_cast<void **>(&m_mapping), m_size, 0, VM_ADDRESS_ANY,
        VM_ACCESS_READ | VM_ACCESS_WRITE, 0, m_handle, 0, nullptr);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to map framebuffer: %" PRId32, ret);
        return false;
    }

    ret = kern_vm_map(
        reinterpret_cast<void **>(&m_backbuffer), m_size, 0, VM_ADDRESS_ANY,
        VM_ACCESS_READ | VM_ACCESS_WRITE, VM_MAP_PRIVATE, INVALID_HANDLE, 0,
        "backbuffer");
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to map backbuffer: %" PRId32, ret);
        return false;
    }

    memset(m_mapping, 0, m_size);
    memset(m_backbuffer, 0, m_size);

    g_terminalApp.addEvent(m_handle, KFB_DEVICE_EVENT_REDRAW, this);

    return true;
}

void Framebuffer::handleEvent(const object_event_t &event) {
    assert(event.handle == m_handle);
    assert(event.event == KFB_DEVICE_EVENT_REDRAW);

    memset(m_mapping, 0, m_size);
    memset(m_backbuffer, 0, m_size);

    g_terminalApp.redraw();
}

static inline size_t pixelOffset(const kfb_mode_t &mode, uint16_t x, uint16_t y) {
    return (y * mode.pitch) + (x * mode.bytes_per_pixel);
}

static inline uint32_t convPixel(const kfb_mode_t &mode, uint32_t rgb) {
    return
        (((rgb >> (24 - mode.red_size))   & ((1 << mode.red_size) - 1))   << mode.red_position) |
        (((rgb >> (16 - mode.green_size)) & ((1 << mode.green_size) - 1)) << mode.green_position) |
        (((rgb >> (8  - mode.blue_size))  & ((1 << mode.blue_size) - 1))  << mode.blue_position);
}

static inline void writePixel(const kfb_mode_t &mode, void *dest, uint32_t value) {
    switch (mode.bytes_per_pixel) {
        case 2:
            reinterpret_cast<uint16_t *>(dest)[0] = value;
            break;
        case 3:
            reinterpret_cast<uint8_t *>(dest)[0] = value & 0xff;
            reinterpret_cast<uint8_t *>(dest)[1] = (value >> 8) & 0xff;
            reinterpret_cast<uint8_t *>(dest)[2] = (value >> 16) & 0xff;
            break;
        case 4:
            reinterpret_cast<uint32_t *>(dest)[0] = value;
            break;
    }
}

void Framebuffer::putPixel(uint16_t x, uint16_t y, uint32_t rgb) {
    uint32_t value = convPixel(m_mode, rgb);
    size_t offset  = pixelOffset(m_mode, x, y);

    writePixel(m_mode, m_backbuffer + offset, value);
    writePixel(m_mode, m_mapping + offset, value);
}

void Framebuffer::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t rgb) {
    if (x == 0 && width == m_mode.width && (rgb == 0 || rgb == 0xffffff)) {
        /* Fast path where we can fill a block quickly. */
        memset(
            m_backbuffer + pixelOffset(m_mode, 0, y),
            static_cast<uint8_t>(rgb),
            height * m_mode.pitch);
        memset(
            m_mapping + pixelOffset(m_mode, 0, y),
            static_cast<uint8_t>(rgb),
            height * m_mode.pitch);
    } else {
        uint32_t value = convPixel(m_mode, rgb);

        for (uint16_t i = 0; i < height; i++) {
            /* Fill on the backbuffer then copy in bulk to the framebuffer. */
            size_t offset = pixelOffset(m_mode, x, y + i);
            uint8_t *dest = m_backbuffer + offset;

            switch (m_mode.bytes_per_pixel) {
                case 2:
                    for (uint16_t j = 0; j < width; j++) {
                        reinterpret_cast<uint16_t *>(dest)[0] = (uint16_t)value;
                        dest += 2;
                    }
                    break;
                case 3:
                    for (uint16_t j = 0; j < width; j++) {
                        (reinterpret_cast<uint8_t *>(dest))[0] = value & 0xff;
                        (reinterpret_cast<uint8_t *>(dest))[1] = (value >> 8) & 0xff;
                        (reinterpret_cast<uint8_t *>(dest))[2] = (value >> 16) & 0xff;
                        dest += 3;
                    }
                    break;
                case 4:
                    for (uint16_t j = 0; j < width; j++) {
                        reinterpret_cast<uint32_t *>(dest)[0] = value;
                        dest += 4;
                    }
                    break;
            }

            memcpy(
                m_mapping + offset,
                m_backbuffer + offset,
                width * m_mode.bytes_per_pixel);
        }
    }
}

void Framebuffer::copyRect(
    uint16_t destX, uint16_t destY, uint16_t srcX, uint16_t srcY,
    uint16_t width, uint16_t height)
{
    if (destX == 0 && srcX == 0 && width == m_mode.width) {
        /* Fast path where we can copy everything in one go. */
        size_t destOffset = pixelOffset(m_mode, 0, destY);
        size_t srcOffset  = pixelOffset(m_mode, 0, srcY);

        /* Copy everything on the backbuffer. */
        memmove(
            m_backbuffer + destOffset,
            m_backbuffer + srcOffset,
            height * m_mode.pitch);

        /* Copy the updated backbuffer onto the framebuffer. */
        memcpy(
            m_mapping + destOffset,
            m_backbuffer + destOffset,
            height * m_mode.pitch);
    } else {
        /* Copy line by line. */
        for (uint16_t i = 0; i < height; i++) {
            size_t destOffset = pixelOffset(m_mode, destX, destY + i);
            size_t srcOffset  = pixelOffset(m_mode, srcX, srcY + i);

            /* Copy everything on the backbuffer. */
            memmove(
                m_backbuffer + destOffset,
                m_backbuffer + srcOffset,
                width * m_mode.bytes_per_pixel);

            /* Copy the updated backbuffer onto the framebuffer. */
            memcpy(
                m_mapping + destOffset,
                m_backbuffer + destOffset,
                width * m_mode.bytes_per_pixel);
        }
    }
}
