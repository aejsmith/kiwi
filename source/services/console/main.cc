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

#include <kiwi/EventLoop.h>

#include "Console.h"
#include "Header.h"
#include "InputDevice.h"

using namespace kiwi;

/** Main function for Console.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Process exit code. */
int main(int argc, char **argv) {
	EventLoop loop;

	/* Create the framebuffer object and draw the header on it. */
	Framebuffer fb("/display/0");
	Header::Instance().Draw(fb);

	/* Create the console. */
	Console console(&fb, 0, Header::Instance().Height(), fb.Width(), fb.Height() - Header::Instance().Height());
	console.Run("/system/binaries/dungeon");

	/* Finally create the input device. */
	InputDevice input("/input/0");

	/* Run the event loop. */
	loop.Run();
	return 0;
}
