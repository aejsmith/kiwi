/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Terminal window class.
 */

#include <kiwi/Graphics/Surface.h>

#include <iostream>

#include "TerminalWindow.h"

using namespace kiwi;
using namespace std;

/** Font to use for terminals. */
Font *TerminalWindow::m_font = 0;

/** Create a new terminal window.
 * @param app		Application the window is for.
 * @param cols		Intial number of columns.
 * @param rows		Initial number of rows. */
TerminalWindow::TerminalWindow(TerminalApp *app, int cols, int rows) :
	m_app(app), m_terminal(cols, rows)
{
	int id;

	m_terminal.OnExit.Connect(this, &TerminalWindow::TerminalExited);
	m_terminal.OnUpdate.Connect(this, &TerminalWindow:: TerminalUpdated);

	/* Create the font if necessary. */
	if(!m_font) {
		m_font = new Font("/system/data/fonts/DejaVuSansMono.ttf", 13.0);
	}

	/* Work out the size to give the window. */
	Size size = m_font->GetSize();
	size = Size(size.GetWidth() * cols, size.GetHeight() * rows);
	Resize(size);

	/* Set up the window. The resize event generated by the Resize() call
	 * will draw the window for us. */
	SetTitle("Terminal");
	id = (m_terminal.GetID() % 4) + 1;
	MoveTo(Point(id * 50, id * 75));

	/* Show the window. */
	Show();
}

/** Handle the terminal process exiting.
 * @param status	Exit status of the process. */
void TerminalWindow::TerminalExited(int status) {
	/* TODO. */
	//DeleteLater();
}

/** Update an area in the terminal buffer.
 * @param rect		Area to update. */
void TerminalWindow::TerminalUpdated(Rect rect) {
	cairo_t *context;

	Size font_size = m_font->GetSize();

	/* Create the context. */
	context = cairo_create(GetSurface()->GetCairoSurface());

	/* Draw the characters. */
	for(int y = rect.GetY(); y < (rect.GetY() + rect.GetHeight()); y++) {
		for(int x = rect.GetX(); x < (rect.GetX() + rect.GetWidth()); x++) {
			Point pos(x * font_size.GetWidth(), y * font_size.GetHeight());

			/* Draw the background. */
			cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(context, pos.GetX(), pos.GetY(), font_size.GetWidth(), font_size.GetHeight());
			cairo_set_source_rgba(context, 0, 0, 0, 0.9);
			cairo_fill(context);
			cairo_set_operator(context, CAIRO_OPERATOR_OVER);

			/* Draw the character. */
			if(m_terminal.GetBuffer()[y][x]) {
				m_font->DrawChar(context, m_terminal.GetBuffer()[y][x], pos);
			}
			Update(Rect(pos, font_size));
		}
	}
}

/** Handle a key press event on the window.
 * @param event		Event information structure. */
void TerminalWindow::KeyPressed(const KeyEvent &event) {
	uint32_t modifiers = event.GetModifiers() & (Input::kControlModifier | Input::kShiftModifier);
	if(modifiers == (Input::kControlModifier | Input::kShiftModifier)) {
		/* Handle keyboard shortcuts. */
		switch(event.GetKey()) {
		case INPUT_KEY_N:
			m_app->CreateWindow();
			break;
		}
	} else {
		/* Send the text to the terminal. */
		string text = event.GetText();
		for(auto it = text.begin(); it != text.end(); ++it) {
			m_terminal.Input(*it);
		}
	}
}

/** Handle the window being resized.
 * @param event		Event information structure. */
void TerminalWindow::Resized(const ResizeEvent &event) {
	cairo_t *context;
	int cols, rows;

	/* Compute the new number of columns/rows. */
	Size font_size = m_font->GetSize();
	cols = event.GetSize().GetWidth() / font_size.GetWidth();
	rows = event.GetSize().GetHeight() / font_size.GetHeight();
	m_terminal.Resize(cols, rows);

	/* Create the context to re-render. */
	context = cairo_create(GetSurface()->GetCairoSurface());

	/* Initialise the background. */
	cairo_rectangle(context, 0, 0, event.GetSize().GetWidth(), event.GetSize().GetHeight());
	cairo_set_source_rgba(context, 0, 0, 0, 0.9);
	cairo_fill(context);

	/* Destroy the context and update the window. */
	cairo_destroy(context);
	Update(GetFrame());
}
