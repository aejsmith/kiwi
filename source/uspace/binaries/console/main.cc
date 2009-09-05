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

#include "console.h"
#include "input.h"
#include "ppm.h"

/** Mode to use. */
#define MODE_WIDTH	1024
#define MODE_HEIGHT	768
#define MODE_DEPTH	16

/** Find a display mode.
 * @param modes		Mode list.
 * @param count		Mode count.
 * @param width		Required width.
 * @param height	Required height.
 * @param bpp		Required depth.
 * @return		Pointer to mode structure, or NULL if not found. */
static display_mode_t *display_mode_find(display_mode_t *modes, size_t count, size_t width, size_t height, size_t bpp) {
	size_t i;

	for(i = 0; i < count; i++) {
		if(modes[i].width != width || modes[i].height != height || modes[i].bpp != bpp) {
			continue;
		}

		return &modes[i];
	}

	return NULL;
}

/** Main function for Console.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Process exit code. */
int main(int argc, char **argv) {
	display_mode_t *modes, *mode;
	size_t count = 0, height;
	InputDevice *input;
	Console *console;
	Framebuffer *fb;
	handle_t handle;
	RGB colour;
	int ret;

	if((handle = device_open("/display/0")) < 0) {
		printf("Failed to open display device (%d)\n", handle);
		return -handle;
	}

	/* Fetch a list of display modes. */
	if((ret = device_request(handle, DISPLAY_MODE_COUNT, NULL, 0, &count, sizeof(size_t), NULL)) != 0) {
		printf("Failed to get mode count (%d)\n", ret);
		return -ret;
	} else if(!count) {
		printf("Display device does not have any usable modes.\n");
		return ERR_DEVICE_ERROR;
	} else if(!(modes = new display_mode_t[count])) {
		return ERR_NO_MEMORY;
	} else if((ret = device_request(handle, DISPLAY_MODE_GET, NULL, 0, modes, sizeof(display_mode_t) * count, NULL)) != 0) {
		printf("Failed to get mode list (%d)\n", ret);
		return -ret;
	}

	/* Find the mode we want and set it. */
	if(!(mode = display_mode_find(modes, count, MODE_WIDTH, MODE_HEIGHT, MODE_DEPTH))) {
		printf("Could not find desired display mode!\n");
		return ERR_NOT_FOUND;
	} else if((ret = device_request(handle, DISPLAY_MODE_SET, &mode->id, sizeof(identifier_t), NULL, 0, NULL)) != 0) {
		printf("Failed to set mode (%d)\n", ret);
		return -ret;
	}

	/* Create the framebuffer object. */
	fb = new Framebuffer(handle, mode);
	if((ret = fb->InitCheck()) != 0) {
		delete fb;
		return -ret;
	}

	/* Write the logo to the framebuffer. */
	{
		PPM ppm(logo_ppm, logo_ppm_size);
		ppm.Draw(fb, 0, 0);
		height = ppm.Height();
	}

	/* Draw a line under the logo. */
	colour.r = colour.g = colour.b = 0x55;
	fb->FillRect(0, height + 1, MODE_WIDTH, 1, colour);

	/* Create the console. */
	console = new Console(fb, 0, height + 2, MODE_WIDTH, MODE_HEIGHT - (height + 3));
	if((ret = console->InitCheck()) != 0) {
		delete console;
		delete fb;
		return -ret;
	}

	/* Finally create the input device. */
	input = new InputDevice("/input/input0");
	if((ret = input->InitCheck()) != 0) {
		delete input;
		delete console;
		delete fb;
		return -ret;
	}

	console->Run("failshell");

	/* This thread is not needed any more. */
	delete[] modes;
	thread_exit(0);
}
