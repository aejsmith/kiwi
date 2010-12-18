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
