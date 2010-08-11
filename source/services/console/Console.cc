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
 * @brief		Console application.
 */

#include <drivers/console.h>

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/process.h>

#include <kiwi/Error.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "Console.h"

using namespace kiwi;

extern unsigned char console_font_6x12[];
extern char **environ;

/** Font information. */
#define FONT_DATA	console_font_6x12
#define FONT_WIDTH	6
#define FONT_HEIGHT	12

/** Currently active console. */
Console *Console::m_active = 0;

/** Constructor for the console.
 * @param fb		Framebuffer to use.
 * @param x		X position.
 * @param y		Y position.
 * @param width		Width.
 * @param height	Height. */
Console::Console(Framebuffer *fb, int x, int y, int width, int height) :
	m_id(-1), m_fb(fb), m_buffer(0), m_fb_x(x), m_fb_y(y),
	m_width_px(width), m_height_px(height), m_cursor_x(0), m_cursor_y(0),
	m_cols(width / FONT_WIDTH), m_rows(height / FONT_HEIGHT),
	m_scroll_start(0), m_scroll_end(m_rows - 1)
{
	status_t ret;

	/* Open the console master. */
	handle_t handle;
	ret = device_open("/console/master", &handle);
	if(ret != STATUS_SUCCESS) {
		throw OSError(ret);
	}
	SetHandle(handle);

	/* Obtain a child console. */
	ret = device_request(m_handle, CONSOLE_MASTER_GET_ID, NULL, 0, &m_id, sizeof(m_id), NULL);
	if(ret != STATUS_SUCCESS) {
		throw OSError(ret);
	}

	/* Allocate the back buffer and fill it. */
	m_buffer = new RGB[m_width_px * m_height_px];
	m_fg_colour.r = m_fg_colour.g = m_fg_colour.b = 0xff;
	m_bg_colour.r = m_bg_colour.g = m_bg_colour.b = 0x0;
	memset(m_buffer, 0, m_width_px * m_height_px * sizeof(RGB));

	/* Set us as the active console if necessary. */
	if(!m_active) {
		m_active = this;
	}

	ToggleCursor();
}

/** Destructor for the console. */
Console::~Console() {
	if(m_buffer) {
		delete[] m_buffer;
	}
}

/** Run a program within a console.
 * @param path		Path to program to run.
 * @return		Whether command started successfully. */
bool Console::Run(const char *path) {
	handle_t map[][2] = {
		{ 0, 0 },
		{ 0, 1 },
		{ 0, 2 },
	};
	const char *args[] = { path, NULL };
	char buf[1024];
	status_t ret;

	sprintf(buf, "/console/%d", m_id);

	/* Open handles to the console. */
	ret = device_open(buf, &map[0][0]);
	if(ret != STATUS_SUCCESS) {
		return false;
	}
	ret = device_open(buf, &map[1][0]);
	if(ret != STATUS_SUCCESS) {
		handle_close(map[0][0]);
		return false;
	}
	ret = device_open(buf, &map[2][0]);
	if(ret != STATUS_SUCCESS) {
		handle_close(map[1][0]);
		handle_close(map[0][0]);
		return false;
	}

	/* Make the handles inheritable so children of the process get them. */
	handle_set_flags(map[0][0], HANDLE_INHERITABLE);
	handle_set_flags(map[1][0], HANDLE_INHERITABLE);
	handle_set_flags(map[2][0], HANDLE_INHERITABLE);

	ret = process_create(path, args, environ, 0, map, 3, NULL);
	handle_close(map[2][0]);
	handle_close(map[1][0]);
	handle_close(map[0][0]);
	if(ret != STATUS_SUCCESS) {
		printf("Could not start process (%d)\n", ret);
		return false;
	}

	return true;
}

/** Add input to the console.
 * @param ch		Input character. */
void Console::Input(unsigned char ch) {
	device_write(m_handle, &ch, 1, 0, NULL);
}

/** Output a character to the console.
 * @param ch		Output character. */
void Console::Output(unsigned char ch) {
	/* No output processing yet. */
	PutChar(ch);
}

/** Redraw the console. */
void Console::Redraw(void) {
	if(m_active) {
		m_fb->DrawRect(m_fb_x, m_fb_y, m_width_px, m_height_px, m_buffer);
	}
}

/** Invert the cursor state at the current position. */
void Console::ToggleCursor(void) {
	int i, j, x, y, off;

	x = m_cursor_x * FONT_WIDTH;
	y = m_cursor_y * FONT_HEIGHT;
	
	for(i = 0; i < FONT_HEIGHT; i++) {
		for(j = 0; j < FONT_WIDTH; j++) {
			off = (m_width_px * (y + i)) + x + j;
			m_buffer[off].r = ~m_buffer[off].r;
			m_buffer[off].g = ~m_buffer[off].g;
			m_buffer[off].b = ~m_buffer[off].b;

			if(m_active == this) {
				m_fb->PutPixel(m_fb_x + x + j, m_fb_y + y + i, m_buffer[off]);
			}
		}
	}
}

/** Put a character on the console.
 * @param ch		Character to place. */
void Console::PutChar(unsigned char ch) {
	int x, y, i, j, off;

	ToggleCursor();

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(m_cursor_x) {
			m_cursor_x--;
		} else if(m_cursor_y) {
			m_cursor_x = m_cols - 1;
			m_cursor_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		m_cursor_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		m_cursor_x = 0;
		m_cursor_y++;
		break;
	case '\t':
		m_cursor_x += 8 - (m_cursor_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		x = m_cursor_x * FONT_WIDTH;
		y = m_cursor_y * FONT_HEIGHT;

		for(i = 0; i < FONT_HEIGHT; i++) {
			for(j = 0; j < FONT_WIDTH; j++) {
				off = (m_width_px * (y + i)) + x + j;
				if(FONT_DATA[(ch * FONT_HEIGHT) + i] & (1<<(7-j))) {
					m_buffer[off] = m_fg_colour;
				} else {
					m_buffer[off] = m_bg_colour;
				}

				/* Update the framebuffer if we are the active
				 * console. */
				if(m_active == this) {
					m_fb->PutPixel(m_fb_x + x + j, m_fb_y + y + i, m_buffer[off]);
				}
			}
		}

		m_cursor_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(m_cursor_x >= m_cols) {
		m_cursor_x = 0;
		m_cursor_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(m_cursor_y >= m_rows) {
		ScrollDown();
		m_cursor_y = m_rows - 1;
	}

	ToggleCursor();
}

/** Clear the console. */
void Console::Clear(void) {
	for(int i = 0; i < (m_width_px * m_height_px); i++) {
		m_buffer[i] = m_bg_colour;
	}

	Redraw();
	ToggleCursor();
}

/** Scroll up one line. */
void Console::ScrollUp(void) {
	size_t row = m_width_px * FONT_HEIGHT;
	size_t pixels = (m_width_px * FONT_HEIGHT) * (m_scroll_end - m_scroll_start);
	memmove(&m_buffer[row * (m_scroll_start + 1)], &m_buffer[row * m_scroll_start], pixels * sizeof(RGB));

	/* Fill the first row with blanks. */
	for(int i = 0; i < (FONT_HEIGHT * m_width_px); i++) {
		m_buffer[(m_scroll_start * row) + i] = m_bg_colour;
	}

	Redraw();
}

/** Scroll down one line. */
void Console::ScrollDown(void) {
	size_t row = m_width_px * FONT_HEIGHT;
	size_t pixels = (m_width_px * FONT_HEIGHT) * (m_scroll_end - m_scroll_start);
	memcpy(&m_buffer[row * m_scroll_start], &m_buffer[row * (m_scroll_start + 1)], pixels * sizeof(RGB));

	/* Fill the last row with blanks. */
	for(int i = 0; i < (FONT_HEIGHT * m_width_px); i++) {
		m_buffer[(m_scroll_end * row) + i] = m_bg_colour;
	}

	Redraw();
}

/** Register events with the event loop. */
void Console::RegisterEvents() {
	RegisterEvent(DEVICE_EVENT_READABLE);
}

/** Event callback function.
 * @param event		Event number received. */
void Console::EventReceived(int event) {
	assert(event == DEVICE_EVENT_READABLE);

	unsigned char ch;
	size_t bytes;
	status_t ret = device_read(m_handle, &ch, 1, 0, &bytes);
	if(ret != STATUS_SUCCESS || bytes != 1) {
		return;
	}

	Output(ch);
}
