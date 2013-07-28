/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Display class.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/vm.h>

#include <kiwi/Support/Utility.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <exception>

#include "Compositor.h"
#include "Display.h"
#include "Session.h"
#include "ServerSurface.h"
#include "WindowServer.h"

using namespace kiwi;
using namespace std;

/** Work out the bytes per pixel for a format.
 * @param format	Pixel format.
 * @return		Number of bytes per pixel. */
static size_t bytes_per_pixel(pixel_format_t format) {
	switch(format) {
	case PIXEL_FORMAT_ARGB32:
	case PIXEL_FORMAT_BGRA32:
	case PIXEL_FORMAT_RGB32:
	case PIXEL_FORMAT_BGR32:
		return 4;
	case PIXEL_FORMAT_RGB24:
	case PIXEL_FORMAT_BGR24:
		return 3;
	case PIXEL_FORMAT_ARGB16:
	case PIXEL_FORMAT_BGRA16:
	case PIXEL_FORMAT_RGB16:
	case PIXEL_FORMAT_BGR16:
		return 2;
	case PIXEL_FORMAT_RGB15:
	case PIXEL_FORMAT_BGR15:
		return 2;
	case PIXEL_FORMAT_IDX8:
	case PIXEL_FORMAT_GREY8:
		return 1;
	}

	clog << "Display has invalid pixel format" << endl;
	abort();
}

/** Work out the pixman format for a pixel format.
 * @param format	Pixel format.
 * @return		Corresponding pixman format. */
static pixman_format_code_t pixman_format_for_format(pixel_format_t format) {
	switch(format) {
	case PIXEL_FORMAT_ARGB32:
		return PIXMAN_a8r8g8b8;
	case PIXEL_FORMAT_BGRA32:
		return PIXMAN_b8g8r8a8;
	case PIXEL_FORMAT_RGB32:
		return PIXMAN_x8r8g8b8;
	case PIXEL_FORMAT_BGR32:
		return PIXMAN_b8g8r8x8;
	case PIXEL_FORMAT_RGB24:
		return PIXMAN_r8g8b8;
	case PIXEL_FORMAT_BGR24:
		return PIXMAN_b8g8r8;
	case PIXEL_FORMAT_ARGB16:
		return PIXMAN_a1r5g5b5;
	case PIXEL_FORMAT_RGB16:
		return PIXMAN_r5g6b5;
	case PIXEL_FORMAT_BGR16:
		return PIXMAN_b5g6r5;
	case PIXEL_FORMAT_RGB15:
		return PIXMAN_x1r5g5b5;
	case PIXEL_FORMAT_BGRA16:
	case PIXEL_FORMAT_BGR15:
		/* Eh? Pixman doesn't support these. */
		clog << "Display has unsupported pixel format" << endl;
		abort();
	case PIXEL_FORMAT_IDX8:
	case PIXEL_FORMAT_GREY8:
		clog << "8-bit surfaces not implemented" << endl;
		abort();
	}

	clog << "Display has invalid pixel format" << endl;
	abort();
}

/** Display constructor.
 * @param server	Window server the display is for.
 * @param path		Device tree path to display. */
Display::Display(WindowServer *server, const char *path) :
	m_server(server), m_mapping(0), m_mapping_size(0), m_image(0)
{
	handle_t handle;
	status_t ret;
	size_t count;

	/* Open the device. */
	ret = kern_device_open(path, DEVICE_RIGHT_READ | DEVICE_RIGHT_WRITE, &handle);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to open display device " << path << " (" << ret << ")" << endl;
		throw exception();
	}
	SetHandle(handle);

	/* Get mode information. */
	ret = kern_device_request(m_handle, DISPLAY_MODE_COUNT, NULL, 0, &count, sizeof(count), NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to get mode count for " << path << " (" << ret << ")" << endl;
		throw exception();
	} else {
		unique_ptr<display_mode_t []> modes(new display_mode_t[count]);
		ret = kern_device_request(m_handle, DISPLAY_GET_MODES, NULL, 0, modes.get(),
		                          sizeof(display_mode_t) * count, NULL);
		if(ret != STATUS_SUCCESS) {
			clog << "Failed to get modes for " << path << " (" << ret << ")" << endl;
			throw exception();
		}

		copy(&modes[0], &modes[count], back_inserter(m_modes));
	}

	/* Try to get the preferred display mode. */
	ret = kern_device_request(m_handle, DISPLAY_GET_PREFERRED_MODE, NULL, 0, &m_current_mode,
	                          sizeof(display_mode_t), NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to get preferred mode for " << path << " (" << ret << ")" << endl;
		throw exception();
	}

	/* Set it and map the framebuffer. */
	ret = SetMode(m_current_mode);
	if(ret != STATUS_SUCCESS) {
		throw exception();
	}
}

/** Set the display mode.
 * @param mode		Mode to set.
 * @return		Status code describing result of the operation. */
status_t Display::SetMode(display_mode_t &mode) {
	status_t ret;

	/* Unmap the current framebuffer if necessary. */
	if(m_image) {
		pixman_image_unref(m_image);
		m_image = 0;
	}
	if(m_mapping) {
		kern_vm_unmap(m_mapping, m_mapping_size);
		m_mapping = 0;
	}

	/* Set the mode. */
	ret = kern_device_request(m_handle, DISPLAY_SET_MODE, &mode.id, sizeof(mode.id), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to set display mode (" << ret << ")" << endl;
		return ret;
	}
	m_current_mode = mode;

	/* Work out the size of the mapping to make. */
	m_mapping_size = p2align(mode.width * mode.height * bytes_per_pixel(mode.format), 0x1000);

	/* Create a mapping for the framebuffer and clear it. */
	ret = kern_vm_map(0, m_mapping_size, VM_MAP_READ | VM_MAP_WRITE, m_handle, mode.offset, &m_mapping);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to map display framebuffer (" << ret << ")" << endl;
		return ret;
	}

	/* Create the pixman image used to draw to the framebuffer. */
	m_image = pixman_image_create_bits(pixman_format_for_format(mode.format),
	                                   mode.width, mode.height,
	                                   reinterpret_cast<uint32_t *>(m_mapping),
	                                   mode.width * bytes_per_pixel(mode.format));
	if(!m_image) {
		clog << "Failed to create pixman image for framebuffer" << endl;
		return STATUS_NO_MEMORY;
	}

	return STATUS_SUCCESS;
}

/** Draw part of a surface onto the framebuffer.
 * @param surface	Surface to draw.
 * @param dest		Position to draw at.
 * @param src		Position to draw from in source surface.
 * @param size		Size of area to draw. */
void Display::DrawSurface(ServerSurface *surface, Point dest, Point src, Size size) {
	/* Pixman handles sanitising all parameters. Use the source operator
	 * as we just want to stick the source surface over the framebuffer,
	 * compositing is done by Compositor. */
	pixman_image_composite(PIXMAN_OP_SRC, surface->GetPixmanImage(), NULL,
	                       m_image, src.GetX(), src.GetY(), 0, 0, dest.GetX(),
	                       dest.GetY(), size.GetWidth(), size.GetHeight());
}

/** Register events with the event loop. */
void Display::RegisterEvents() {
	RegisterEvent(DISPLAY_EVENT_REDRAW);
}

/** Event callback function.
 * @param event		Event number. */
void Display::HandleEvent(int event) {
	assert(event == DISPLAY_EVENT_REDRAW);
	m_server->GetActiveSession()->GetCompositor()->Redraw();
}
