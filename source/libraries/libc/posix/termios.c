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
 * @brief		Terminal control functions.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "../libc.h"

/** Get the input baud rate from a termios structure.
 * @param tio		Terminal I/O settings structure.
 * @return		Input baud rate. */
speed_t cfgetispeed(const struct termios *tio) {
	return tio->c_ispeed;
}

/** Get the output baud rate from a termios structure.
 * @param tio		Terminal I/O settings structure.
 * @return		Output baud rate. */
speed_t cfgetospeed(const struct termios *tio) {
	return tio->c_ospeed;
}

/** Set the input baud rate in a termios structure.
 *
 * Sets the input baud rate in a termios structure. This will have no effect on
 * the settings of a terminal until the attributes are set using tcsetattr().
 *
 * @param tio		Terminal I/O settings structure.
 * @param speed		Input baud rate to set.
 *
 * @return		0 on success, -1 on failure.
 */
int cfsetispeed(struct termios *tio, speed_t speed) {
	tio->c_ispeed = speed;
	return 0;
}

/** Set the output baud rate in a termios structure.
 *
 * Sets the output baud rate in a termios structure. This will have no effect
 * on the settings of a terminal until the attributes are set using tcsetattr().
 *
 * @param tio		Terminal I/O settings structure.
 * @param speed		Output baud rate to set.
 *
 * @return		0 on success, -1 on failure.
 */
int cfsetospeed(struct termios *tio, speed_t speed) {
	tio->c_ospeed = speed;
	return 0;
}

/** Wait until all data written to a terminal is transmitted.
 * @param fd		File descriptor for terminal.
 * @return		0 on success, -1 on failure. */
int tcdrain(int fd) {
	status_t ret;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TIOCDRAIN, NULL, 0, NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Suspend or restart data transmission on a terminal.
 * @param fd		File descriptor for terminal.
 * @param action	Action to perform. TCOOFF will suspend output, TCOON
 *			will resume output. TCIOFF will suspend input, TCION
 *			will resume input.
 * @return		0 on success, -1 on failure. */
int tcflow(int fd, int action) {
	status_t ret;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TCXONC, &action, sizeof(action), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Discard the contents of terminal buffers.
 * @param fd		File descriptor for terminal.
 * @param action	Action to perform. TCIFLUSH will flush data received
 *			but not read. TCOFLUSH will flush data written but not
 *			transmitted. TCIOFLUSH will flush both.
 * @return		0 on success, -1 on failure. */
int tcflush(int fd, int action) {
	status_t ret;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TCFLSH, &action, sizeof(action), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Get I/O settings for a terminal.
 * @param fd		File descriptor for terminal.
 * @param tiop		Structure to fill with settings.
 * @return		0 on success, -1 on failure. */
int tcgetattr(int fd, struct termios *tiop) {
	status_t ret;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TCGETA, NULL, 0, tiop, sizeof(*tiop), NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Get process group ID of the terminal's session.
 * @param fd		File descriptor for terminal.
 * @return		Process group ID on success, -1 on failure. */
pid_t tcgetsid(int fd) {
	errno = ENOSYS;
	return -1;
}

/** Send a break for specified duration.
 * @param fd		File descriptor for terminal.
 * @param duration	Duration of break.
 * @return		0 on success, -1 on failure. */
int tcsendbreak(int fd, int duration) {
	errno = ENOSYS;
	return -1;
}

/** Set I/O settings for a terminal.
 * @param fd		File descriptor for terminal.
 * @param action	How to perform the change. If TCSANOW, the change will
 *			occur immediately. If TCSADRAIN, the change will occur
 *			after all written data is transmitted. If TCSAFLUSH,
 *			the change will occur after all written data is
 *			transmitted, and waiting input will be discarded.
 * @param tiop		Structure containing new terminal settings.
 * @return		0 on success, -1 on failure. */
int tcsetattr(int fd, int action, const struct termios *tio) {
	status_t ret;
	int request;

	if(!isatty(fd)) {
		return -1;
	}

	switch(action) {
	case TCSANOW:
		request = TCSETA;
		break;
	case TCSADRAIN:
		request = TCSETAW;
		break;
	case TCSAFLUSH:
		request = TCSETAF;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	ret = device_request(fd, request, tio, sizeof(*tio), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Get the foreground process group of a terminal
 * @param fd		File descriptor for terminal.
 * @return		Process group ID on success, -1 on failure. */
pid_t tcgetpgrp(int fd) {
	status_t ret;
	pid_t pgid;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TIOCGPGRP, NULL, 0, &pgid, sizeof(pgid), NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return pgid;
}

/** Set the foreground process group of a terminal
 * @param fd		File descriptor for terminal.
 * @param pgid		Process group ID to set.
 * @return		0 on success, -1 on failure. */
int tcsetpgrp(int fd, pid_t pgid) {
	status_t ret;

	if(!isatty(fd)) {
		return -1;
	}

	ret = device_request(fd, TIOCSPGRP, &pgid, sizeof(pgid), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
