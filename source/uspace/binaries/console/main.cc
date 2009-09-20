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

#include <kernel/device.h>
#include <kernel/errors.h>
#include <kernel/handle.h>
#include <kernel/thread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Console.h"
#include "EventLoop.h"
#include "Header.h"
#include "InputDevice.h"

/** Main function for Console.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Process exit code. */
int main(int argc, char **argv) {
	InputDevice *input;
	Console *console;
	Framebuffer *fb;

	/* Create the framebuffer object. */
	fb = new Framebuffer("/display/0");
	if(!fb->Initialised()) {
		delete fb;
		return 1;
	}

	/* Draw the header. */
	Header::Instance()->Draw(fb);

	/* Create the console. */
	console = new Console(fb, 0, Header::Instance()->Height(), fb->Width(), fb->Height() - Header::Instance()->Height());
	if(!console->Initialised()) {
		delete console;
		delete fb;
		return 1;
	}

	/* Finally create the input device. */
	input = new InputDevice("/input/0");
	if(!input->Initialised()) {
		delete input;
		delete console;
		delete fb;
		return 1;
	}

	console->Run("failshell");

	EventLoop::Instance()->Run();
	return 0;
}
