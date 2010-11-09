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
TerminalApp::TerminalApp(int argc, char **argv) :
	m_window_count(0)
{
	/* Change to our home directory. */
	chdir(getenv("HOME"));

	/* Create the initial terminal. */
	CreateWindow();
}

/** Create a new terminal window. */
void TerminalApp::CreateWindow() {
	TerminalWindow *window = new TerminalWindow(this, 100, 35);
	m_window_count++;
	window->OnDestroy.Connect(this, &TerminalApp::WindowDestroyed);
	window->GetTerminal().Run("/system/binaries/bash");
}

/** Handle destruction of a window. */
void TerminalApp::WindowDestroyed(Object *obj) {
	if(--m_window_count == 0) {
		Quit();
	}
}

/** Main function of the terminal emulator. */
int main(int argc, char **argv) {
	TerminalApp app(argc, argv);
	return app.Run();
}
