/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Framebuffer class.
 */

#include <kernel/device.h>
#include <kernel/errors.h>
#include <kernel/object.h>
#include <kernel/vm.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Console.h"
#include "Framebuffer.h"
#include "Header.h"

/** Get red colour from RGB value. */
#define RED(x, bits)		((x >> (24 - bits)) & ((1 << bits) - 1))

/** Get green colour from RGB value. */
#define GREEN(x, bits)		((x >> (16 - bits)) & ((1 << bits) - 1))

/** Get blue colour from RGB value. */
#define BLUE(x, bits)		((x >> (8  - bits)) & ((1 << bits) - 1))

/** Get a pixel from a 16-bit (5:6:5) framebuffer.
 * @param src		Source location for pixel.
 * @return		RGB colour of pixel. */
static inline uint32_t getpixel_565(uint16_t *src) {
	uint32_t ret = 0;

	ret |= (((src[0] >> 11) & 0x1f) << (16 + 3));
	ret |= (((src[0] >> 5) & 0x3f) << (8 + 2));
	ret |= ((src[0] & 0x1f) << 3);
	return ret;
}

/** Draw a pixel on a 16-bit (5:6:5) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void putpixel_565(uint16_t *dest, uint32_t colour) {
	*dest = (RED(colour, 5) << 11) | (GREEN(colour, 6) << 5) | BLUE(colour, 5);
}

/** Get a pixel from a 24-bit (8:8:8) framebuffer.
 * @param src		Source location for pixel.
 * @return		RGB colour of pixel. */
static inline uint32_t getpixel_888(uint8_t *src) {
	return (((src[0] << 16) & 0xff0000) | ((src[1] << 8) & 0xff00) | (src[2] & 0xff));
}

/** Draw a pixel on a 24-bit (8:8:8) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void putpixel_888(uint8_t *dest, uint32_t colour) {
	dest[2] = RED(colour, 8);
	dest[1] = GREEN(colour, 8);
	dest[0] = BLUE(colour, 8);
}

/** Get a pixel from a 32-bit (0:8:8:8) framebuffer.
 * @param src		Source location for pixel.
 * @return		RGB colour of pixel. */
static inline uint32_t getpixel_0888(uint32_t *src) {
	return (*src & 0xffffff);
}

/** Draw a pixel on a 32-bit (0:8:8:8) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void putpixel_0888(uint32_t *dest, uint32_t colour) {
	*dest = colour;
}

/** Constructor for a Framebuffer object.
 * @param device	Path to device to use. */
Framebuffer::Framebuffer(const char *device) :
	m_init_status(0), m_buffer(0)
{
	display_mode_t mode;
	int ret;

	handle_t handle = device_open(device);
	if(handle < 0) {
		printf("Failed to open display device (%d)\n", m_handle);
		m_init_status = m_handle;
		return;
	}
	setHandle(handle);

	/* Try to get the preferred display mode. */
	if((ret = device_request(m_handle, DISPLAY_GET_PREFERRED_MODE, NULL, 0,
	                         &mode, sizeof(display_mode_t), NULL)) != 0) {
		printf("Failed to get preferred display mode (%d)\n", ret);
		m_init_status = ret;
		return;
	}

	/* Set the mode. */
	if((ret = device_request(m_handle, DISPLAY_SET_MODE, &mode.id, sizeof(mode.id), NULL, 0, NULL)) != 0) {
		printf("Failed to set mode (%d)\n", ret);
		m_init_status = ret;
		return;
	}

	/* Store mode details. */
	m_width = mode.width;
	m_height = mode.height;
	m_depth = mode.depth;

	/* Work out the size of the mapping to make. */
	m_buffer_size = m_width * m_height * (m_depth / 8);
	if(m_buffer_size % 0x1000) {
		m_buffer_size += 0x1000;
		m_buffer_size -= m_buffer_size % 0x1000;
	}

	/* Create a mapping for the framebuffer and clear it. */
	if((ret = vm_map(0, m_buffer_size, VM_MAP_READ | VM_MAP_WRITE, m_handle, mode.offset,
	                 reinterpret_cast<void **>(&m_buffer))) != 0) {
		m_init_status = ret;
		return;
	}
	memset(m_buffer, 0, m_buffer_size);
}

/** Framebuffer destructor. */
Framebuffer::~Framebuffer() {
	if(m_buffer) {
		vm_unmap(m_buffer, m_buffer_size);
	}
}

/** Get a pixel from the screen.
 * @param x		X position.
 * @param y		Y position.
 * @return		Pixel colour. */
RGB Framebuffer::GetPixel(int x, int y) {
	uint8_t *src = m_buffer + (((y * m_width) + x) * (m_depth / 8));
	uint32_t ret = 0;
	RGB rgb;

	switch(m_depth) {
	case 16:
		ret = getpixel_565(reinterpret_cast<uint16_t *>(src));
		break;
	case 24:
		ret = getpixel_888(src);
		break;
	case 32:
		ret = getpixel_0888(reinterpret_cast<uint32_t *>(src));
		break;
	}

	rgb.r = RED(ret, 8);
	rgb.g = GREEN(ret, 8);
	rgb.b = BLUE(ret, 8);
	return rgb;
}

/** Put a pixel on the screen.
 * @param x		X position.
 * @param y		Y position.
 * @param colour	Colour to write in. */
void Framebuffer::PutPixel(int x, int y, RGB colour) {
	uint8_t *dest = m_buffer + (((y * m_width) + x) * (m_depth / 8));
	uint32_t rgb;

	rgb = (colour.r << 16) | (colour.g << 8) | colour.b;
	switch(m_depth) {
	case 16:
		putpixel_565(reinterpret_cast<uint16_t *>(dest), rgb);
		break;
	case 24:
		putpixel_888(dest, rgb);
		break;
	case 32:
		putpixel_0888(reinterpret_cast<uint32_t *>(dest), rgb);
		break;
	}
}

/** Fill an area with a solid colour.
 * @param x		X position to start at.
 * @param y		Y position to start at.
 * @param width		Width of rectangle.
 * @param height	Height of rectangle.
 * @param colour	Colour to use. */
void Framebuffer::FillRect(int x, int y, int width, int height, RGB colour) {
	int i, j;

	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			PutPixel(x + j, y + i, colour);
		}
	}
}

/** Write a rectangle to the screen.
 * @param x		X position to start at.
 * @param y		Y position to start at.
 * @param width		Width of rectangle.
 * @param height	Height of rectangle.
 * @param buffer	Buffer containing rectangle to write. */
void Framebuffer::DrawRect(int x, int y, int width, int height, RGB *buffer) {
	int i, j;

	for(i = 0; i < height; i++) {
		for(j = 0; j < width; j++) {
			PutPixel(x + j, y + i, buffer[(i * width) + j]);
		}
	}
}

/** Register events with the event loop. */
void Framebuffer::registerEvents() {
	registerEvent(DISPLAY_EVENT_REDRAW);
}

/** Event callback function.
 * @param event		Event number. */
void Framebuffer::eventReceived(int event) {
	assert(event == DISPLAY_EVENT_REDRAW);

	Header::Instance()->Draw(this);
	Console::GetActive()->Redraw();
}
