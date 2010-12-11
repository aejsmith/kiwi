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
 * @brief		Device control function.
 */

#include <kernel/device.h>
#include <kernel/status.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>

#include "../libc.h"

/** Control a device.
 * @note		The Kiwi implementation of this function only works for
 *			terminal devices, and will return an error if used on
 *			anything else.
 * @param fd		File descriptor to device.
 * @param request	Request to perform.
 * @param ...		Optional argument pointer.
 * @return		Request-dependant return code, or -1 for an error. */
int ioctl(int fd, int request, ...) {
	void *in = NULL, *out = NULL;
	size_t insz = 0, outsz = 0;
	status_t ret;
	va_list args;
	int arg;

	if(!isatty(fd)) {
		errno = ENOTSUP;
		return -1;
	}

	va_start(args, request);

	/* Convert the request and arguments to what the kernel wants. */
	switch(request) {
	case TIOCDRAIN:
		break;
	case TCXONC:
	case TCFLSH:
		arg = va_arg(args, int);
		in = &arg;
		insz = sizeof(arg);
		break;
	case TCGETA:
		out = va_arg(args, void *);
		outsz = sizeof(struct termios);
		break;
	case TCSETA:
	case TCSETAW:
	case TCSETAF:
		in = va_arg(args, void *);
		insz = sizeof(struct termios);
		break;
	case TIOCGPGRP:
		in = va_arg(args, void *);
		insz = sizeof(int);
		break;
	case TIOCSPGRP:
		out = va_arg(args, void *);
		outsz = sizeof(int);
		break;
	case TIOCGWINSZ:
		out = va_arg(args, void *);
		outsz = sizeof(struct winsize);
		break;
	case TIOCSWINSZ:
		in = va_arg(args, void *);
		insz = sizeof(struct winsize);
		break;
	default:
		errno = EINVAL;
		break;
	}

	va_end(args);

	/* Perform the request. */
	ret = kern_device_request(fd, request, in, insz, out, outsz, NULL);
	if(ret != STATUS_SUCCESS) {
		if(ret == STATUS_INVALID_REQUEST) {
			errno = ENOTTY;
		} else {
			libc_status_to_errno(ret);
		}
		return -1;
	}

	return 0;
}
