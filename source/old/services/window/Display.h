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

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <drivers/display.h>

#include <kiwi/Graphics/Point.h>
#include <kiwi/Graphics/Size.h>
#include <kiwi/Handle.h>

#include <pixman.h>
#include <vector>

class ServerSurface;
class WindowServer;

/** Class representing a display. */
class Display : public kiwi::Handle {
public:
	/** Type of the mode vector. */
	typedef std::vector<display_mode_t> ModeVector;

	Display(WindowServer *server, const char *path);

	status_t SetMode(display_mode_t &mode);
	void DrawSurface(ServerSurface *surface, kiwi::Point dest, kiwi::Point src, kiwi::Size size);

	/** Get an array of modes supported by the device.
	 * @return		Reference to mode vector. */
	const ModeVector &GetModes() const { return m_modes; }

	/** Get the current mode the device is using.
	 * @return		Reference to current mode. */
	const display_mode_t &GetCurrentMode() const { return m_current_mode; }

	/** Get the size of the current mode.
	 * @return		Size of the current mode. */
	kiwi::Size GetSize() const {
		return kiwi::Size(m_current_mode.width, m_current_mode.height);
	}
private:
	void RegisterEvents();
	void HandleEvent(int event);

	WindowServer *m_server;		/**< Server that the display is for. */
	ModeVector m_modes;		/**< Modes supported by the device. */
	display_mode_t m_current_mode;	/**< Current mode set on the device. */
	void *m_mapping;		/**< Framebuffer mapping. */
	size_t m_mapping_size;		/**< Size of the framebuffer mapping. */
	pixman_image_t *m_image;	/**< Image referring to the framebuffer. */
};

#endif /* __DISPLAY_H */
