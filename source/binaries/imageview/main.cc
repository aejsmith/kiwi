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
 * @brief		Image viewer.
 */

#include <cairo/cairo.h>

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Graphics/Surface.h>
#include <kiwi/EventLoop.h>

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Class for an image window. */
class ImageWindow : public kiwi::BaseWindow {
public:
	ImageWindow(cairo_surface_t *surface, const char *title, bool transparent);
private:
	void KeyPressed(const kiwi::KeyEvent &event);
};

/** Constructor for an image window.
 * @param surface	Surface to display.
 * @param title		Title for the window.
 * @param border	Whether the window should be transparent. */
ImageWindow::ImageWindow(cairo_surface_t *surface, const char *title, bool transparent) :
	BaseWindow((transparent) ? kBorderlessStyle : kNormalStyle)
{
	cairo_t *context;

	/* Set up the window. */
	Resize(kiwi::Size(cairo_image_surface_get_width(surface), cairo_image_surface_get_height(surface)));
	SetTitle(title);

	/* Draw the background and image. */
	context = cairo_create(GetSurface()->GetCairoSurface());
	if(transparent) {
		cairo_set_source_rgba(context, 1, 1, 1, 0);
	} else {
		cairo_set_source_rgb(context, 1, 1, 1);
	}
	cairo_fill(context);
	cairo_set_source_surface(context, surface, 0, 0);
	cairo_paint(context);
	cairo_destroy(context);

	/* Show the window. */
	Show();
}

/** Handle a key press on an image window. */
void ImageWindow::KeyPressed(const kiwi::KeyEvent &event) {
	if(event.GetKey() == INPUT_KEY_ESC) {
		DeleteLater();
	}
}

/** Image viewer application class. */
class ImageViewer : public kiwi::EventLoop {
public:
	ImageViewer(int argc, char **argv);
private:
	/** Handle the window being closed. */
	void WindowClosed(Object *obj) { Quit(); }

	/** Parse command line arguments. */
	bool ParseArguments(int argc, char **argv, const char *&path, bool &transparent) {
		int i = 1;

		transparent = false;
		if(i < argc) {
			if(strcmp(argv[i], "-t") == 0) {
				transparent = true;
				i++;
			}
			if(i < argc) {
				path = argv[i];
				return true;
			}
		}

		return false;
	}
};

/** Image viewer application constructor. */
ImageViewer::ImageViewer(int argc, char **argv) {
	cairo_surface_t *surface;
	ImageWindow *window;
	const char *path;
	bool transparent;

	/* Parse arguments. */
	if(!ParseArguments(argc, argv, path, transparent)) {
		std::cerr << "Usage: " << argv[0] << " [-t] <image>" << std::endl;
		exit(EXIT_FAILURE);
	}

	/* Load the image. */
	surface = cairo_image_surface_create_from_png(path);
	if(cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		std::cerr << "Failed to load image: ";
		std::cerr << cairo_status_to_string(cairo_surface_status(surface)) << std::endl;
		exit(EXIT_FAILURE);
	}

	/* Create the window. */
	window = new ImageWindow(surface, path, transparent);
	window->OnDestroy.Connect(this, &ImageViewer::WindowClosed);
	cairo_surface_destroy(surface);
}

/** Main function of the image viewer. */
int main(int argc, char **argv) {
	ImageViewer app(argc, argv);
	return app.Run();
}
