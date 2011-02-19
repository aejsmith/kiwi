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
 * @brief		Terminal emulator.
 */

#include <stdlib.h>
#include <unistd.h>

#include "TerminalApp.h"
#include "TerminalWindow.h"

/** Terminal application constructor.
 * @param argc		Argument count.
 * @param argv		Argument array. */
TerminalApp::TerminalApp(int argc, char **argv) {
	/* Change to our home directory and set USER. FIXME: Better place for this. */
	chdir(getenv("HOME"));
	setenv("USER", "admin", 1);

	/* Set the TERM value for clients to inherit. */
	setenv("TERM", "xterm-color", 1);

	/* Create the initial terminal. */
	CreateWindow();
}

/** Create a new terminal window. */
void TerminalApp::CreateWindow() {
	TerminalWindow *window = new TerminalWindow(this, 100, 35);
	m_windows.push_back(window);
	window->OnDestroy.Connect(this, &TerminalApp::WindowDestroyed);
	window->GetTerminal().Run("/system/binaries/bash");
}

/** Perform post-event handling tasks. */
void TerminalApp::PostHandle() {
	for(auto it = m_windows.begin(); it != m_windows.end(); ++it) {
		(*it)->Flush();
	}
}

/** Handle destruction of a window. */
void TerminalApp::WindowDestroyed(Object *obj) {
	m_windows.remove(static_cast<TerminalWindow *>(obj));
	if(m_windows.empty()) {
		Quit();
	}
}

/** Main function of the terminal emulator. */
int main(int argc, char **argv) {
	TerminalApp app(argc, argv);
	return app.Run();
}
