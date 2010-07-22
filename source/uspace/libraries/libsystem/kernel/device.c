/*
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
 * @brief		Device functions.
 */

#include <kernel/device.h>

/** Perform a device-specific operation.
 * @param handle	Handle to device to perform operation on.
 * @param request	Operation number to perform.
 * @param in		Optional input buffer containing data to pass to the
 *			operation handler.
 * @param insz		Size of input buffer.
 * @param out		Optional output buffer. Should be provided if the
 *			operation expects an output buffer.
 * @param outsz		Size of output buffer.
 * @param bytesp	Where to store size of data stored in output buffer.
 * @return		Positive value on success, -1 on failure. */
int device_request(handle_t handle, int request, void *in, size_t insz, void *out, size_t outsz, size_t *bytesp) {
	device_request_args_t args = {
		.handle = handle,
		.request = request,
		.in = in,
		.insz = insz,
		.out = out,
		.outsz = outsz,
		.bytesp = bytesp,
	};
	return _device_request(&args);
}
