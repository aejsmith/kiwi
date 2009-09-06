/* Kiwi console application
 * Copyright (C) 2009 Alex Smith
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
#include <kernel/handle.h>
#include <kernel/thread.h>

#include <kiwi/Process.h>

#include <stdio.h>
#include <string.h>

#include "console.h"

using namespace kiwi;

extern unsigned char console_font_8x8[2048];

/** Font information. */
#define FONT_DATA	console_font_8x8
#define FONT_WIDTH	8
#define FONT_HEIGHT	8

/** Current active console. */
Console *Console::m_active = 0;

/** Constructor for the console.
 * @param fb		Framebuffer to use.
 * @param x		X position.
 * @param y		Y position.
 * @param width		Width.
 * @param height	Height. */
Console::Console(Framebuffer *fb, int x, int y, int width, int height) :
		m_init_status(0), m_thread(-1), m_master(-1), m_id(-1),
		m_fb(fb), m_buffer(0), m_fb_x(x), m_fb_y(y), m_width_px(width),
		m_height_px(height), m_cursor_x(0), m_cursor_y(0),
		m_cols(width / FONT_WIDTH), m_rows(height / FONT_HEIGHT),
		m_scroll_start(0), m_scroll_end(m_rows - 1) {
	handle_t handle;
	char buf[1024];

	/* Open the console manager and request a console. */
	if((handle = device_open("/console/manager")) < 0) {
		printf("Failed to open console manager (%d)\n", handle);
		m_init_status = handle;
		return;
	} else if((m_id = device_request(handle, CONSOLE_MANAGER_CREATE, NULL, 0, NULL, 0, NULL)) < 0) {
		printf("Failed to create console master (%d)\n", m_id);
		m_init_status = m_id;
		return;
	}
	handle_close(handle);

	/* Open the console master. */
	sprintf(buf, "/console/%d/master", m_id);
	if((m_master = device_open(buf)) < 0) {
		printf("Failed to open console master (%d)\n", m_master);
		m_init_status = m_master;
		return;
	}

	/* Allocate the buffer. */
	m_buffer = new RGB[m_width_px * m_height_px];

	/* Create a thread to receive output. */
	sprintf(buf, "output-%d", m_id);
	if((m_thread = thread_create(buf, NULL, 0, _ThreadEntry, this)) < 0) {
		m_init_status = m_thread;
		return;
	}

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
	if(m_thread >= 0) {
		/* FIXME: Kill thread. */
		handle_close(m_thread);
	}
	if(m_master >= 0) {
		handle_close(m_master);
	}
	if(m_buffer) {
		delete[] m_buffer;
	}
}

/** Run a command within a console.
 * @param cmdline	Command line to run.
 * @return		0 if command started successfully, negative error code
 *			on failure. */
int Console::Run(const char *cmdline) {
	char buf[1024];
	char *env[] = { const_cast<char *>("PATH=/system/binaries"), buf, NULL };
	Process *proc;
	int ret;

	sprintf(buf, "CONSOLE=/console/%d/slave", m_id);

	if((ret = Process::create(proc, cmdline, env, false, true)) != 0) {
		return ret;
	}

	delete proc;
	return 0;
}

/** Add input to the console.
 * @param ch		Input character. */
void Console::Input(unsigned char ch) {
	device_write(m_master, &ch, 1, 0, NULL);
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
	int i;

	for(i = 0; i < (m_width_px * m_height_px); i++) {
		m_buffer[i] = m_bg_colour;
	}

	Redraw();
	ToggleCursor();
}

/** Scroll up one line. */
void Console::ScrollUp(void) {
#if 0
	size_t size, row;
	uint32_t *d, *s;
	int j, k;

	/* Get the size of one row. */
	row = (dev->curr_mode->width * curr_font->height) * BPP_TO_BYTES(dev->curr_mode);

	/* Size to copy. */
	size = row * (cons->scroll_end - cons->scroll_start);

	d = (uint32_t *)(dev->fb_mapping + (row * (cons->scroll_start + 1)));
	s = (uint32_t *)(dev->fb_mapping + (row * cons->scroll_start));

	/* Copy the data */
	memmove(d, s, size);

	/* Fill the top row with blanks. */
	for(j = 0; j < curr_font->height; j++) {
		for(k = 0; k < dev->curr_mode->width; k++) {
			video_console_putpixel(dev, bg, k, (cons->scroll_start * curr_font->height) + j);
		}
	}
#endif
}

/** Scroll down one line. */
void Console::ScrollDown(void) {
	size_t row, pixels;
	int i;

	row = m_width_px * FONT_HEIGHT;
	pixels = (m_width_px * FONT_HEIGHT) * (m_scroll_end - m_scroll_start);
	memcpy(&m_buffer[row * m_scroll_start], &m_buffer[row * (m_scroll_start + 1)], pixels * sizeof(RGB));

	/* Fill the last row with blanks. */
	for(i = 0; i < (FONT_HEIGHT * m_width_px); i++) {
		m_buffer[(m_scroll_end * row) + i] = m_bg_colour;
	}

	Redraw();
}

/** Output thread function.
 * @param arg		Thread argument (console object pointer). */
void Console::_ThreadEntry(void *arg) {
	Console *console = reinterpret_cast<Console *>(arg);
	unsigned char ch;
	size_t bytes;
	int ret;

	while(true) {
		if((ret = device_read(console->m_master, &ch, 1, 0, &bytes)) != 0) {
			printf("Failed to read output (%d)\n", ret);
			continue;
		} else if(bytes != 1) {
			continue;
		}

		console->Output(ch);
	}
}
