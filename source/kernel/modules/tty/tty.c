/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Terminal device manager.
 *
 * @todo		POSIXy stuff like process groups, sessions, signals.
 */

#include <io/device.h>

#include <lib/string.h>

#include <ipc/pipe.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

#include "tty_priv.h"

/** Map an uppercase ASCII character to a control character. */
#define ASCII_CTRL(c)		((c) & 0x1F)

/** Default terminal I/O settings. */
static struct termios termios_defaults = {
	.c_iflag         = ICRNL,
	.c_oflag         = OPOST | ONLCR,
	.c_cflag         = CREAD | CS8 | HUPCL | CLOCAL,
	.c_lflag         = ICANON | IEXTEN | ISIG | ECHO | ECHOE | ECHONL,
	.c_cc            = {
		[VEOF]   = ASCII_CTRL('D'),
		[VEOL]   = _POSIX_VDISABLE,
		[VERASE] = ASCII_CTRL('H'),
		[VINTR]  = ASCII_CTRL('C'),
		[VKILL]  = ASCII_CTRL('U'),
		[VMIN]   = _POSIX_VDISABLE,
		[VQUIT]  = ASCII_CTRL('\\'),
		[VSTART] = ASCII_CTRL('Q'),
		[VSTOP]  = ASCII_CTRL('S'),
		[VSUSP]  = ASCII_CTRL('Z'),
		[VTIME]  = _POSIX_VDISABLE,
		[VLNEXT] = ASCII_CTRL('V'),
	},
	.c_ispeed        = B38400,
	.c_ospeed        = B38400,
};

/** Terminal device directory/terminal master device. */
static device_t *tty_device_dir;
static device_t *tty_master_device;

/** Next terminal ID. */
static atomic_t next_tty_id = 0;

/** Release a terminal device.
 * @param tty		Terminal to release. */
static void tty_release(tty_device_t *tty) {
	if(refcount_dec(&tty->count) == 0) {
		pipe_destroy(tty->output);
		tty_buffer_destroy(tty->input);
		kfree(tty);
	}
}

/** Check if a character is a control character.
 *
 * Checks if the given character is a certain control character for a
 * terminal. If the terminal's next character is escaped, the function
 * always returns false.
 *
 * @param tty		Terminal to check against.
 * @param ch		Character to check.
 * @param cc		Control character we want.
 *
 * @return		True if character is control character, false if not.
 */
static inline bool tty_is_cchar(tty_device_t *tty, uint16_t ch, int cc) {
	if(ch & TTY_CHAR_ESCAPED || ch == _POSIX_VDISABLE) {
		return false;
	}

	return (ch == (uint16_t)tty->termios.c_cc[cc]);
}

/** Echo an input character.
 * @param tty		Terminal to echo on.
 * @param ch		Character to echo.
 * @param raw		If true, take the character as is. */
static void tty_echo(tty_device_t *tty, uint16_t ch, bool raw) {
	char buf[2] = { ch & 0xFF, 0 };
	size_t count = 1;

	/* Don't need to check for escape, the flag should be set in the
	 * character so it won't match. */
	if(!(tty->termios.c_lflag & ECHO)) {
		/* Even if ECHO is not set, newlines should be echoed if
		 * both ECHONL and ICANON are set. */
		if(buf[0] != '\n' || (tty->termios.c_lflag & (ECHONL | ICANON)) != (ECHONL | ICANON)) {
			return;
		}
	}

	if(!raw && (ch & 0xFF) < ' ') {
		if(ch & TTY_CHAR_ESCAPED || (buf[0] != '\n' && buf[0] != '\r' && buf[0] != '\t')) {
			/* Print it as ^ch. */
			buf[0] = '^';
			buf[1] = '@' + (ch & 0xFF);
			count++;
		}
	}

	/* We cannot block here: if a thread writes to the terminal and blocks
	 * to wait for space, and the terminal master tries to give input to
	 * the terminal, a deadlock would occur if this blocks. TODO: Offload
	 * to a DPC or something if it would block. */
	pipe_write(tty->output, buf, count, true, NULL);
}

/** Add a character to a terminal's input buffer.
 * @param tty		Terminal to add to. Should be locked.
 * @param value		Character to add.
 * @param nonblock	Whether to allow blocking.
 * @return		Status code describing result of the operation. */
static status_t tty_input(tty_device_t *tty, unsigned char value, bool nonblock) {
	uint16_t ch = value;
	size_t erase;

	/* Strip character to 7-bits if required. */
	if(tty->termios.c_iflag & ISTRIP) {
		ch &= 0x007F;
	}

	/* Perform extended processing if required. For now we only support
	 * escaping the next character (VLNEXT). */
	if(tty->termios.c_lflag & IEXTEN) {
		if(tty->escaped) {
			/* Escape the current character. */
			ch |= TTY_CHAR_ESCAPED;
			tty->escaped = false;
		} else if(tty_is_cchar(tty, ch, VLNEXT)) {
			tty->escaped = true;
			return STATUS_SUCCESS;
		}
	}

	/* Handle CR/NL characters. */
	switch(ch) {
	case '\r':
		if(tty->termios.c_iflag & IGNCR) {
			/* Ignore it. */
			return STATUS_SUCCESS;
		} else if(tty->termios.c_iflag & ICRNL) {
			/* Convert it to a newline. */
			ch = '\n';
		}
		break;
	case '\n':
		if(tty->termios.c_iflag & INLCR) {
			/* Convert it to a carriage return. */
			ch = '\r';
		}
		break;
	}

	/* Check for output control characters. */
	if(tty->termios.c_iflag & IXON) {
		if(tty_is_cchar(tty, ch, VSTOP)) {
			tty->inhibited = true;
			return STATUS_SUCCESS;
		} else if(tty->inhibited) {
			/* Restart on any character if IXANY is set, but don't
			 * ignore it. */
			if(tty->termios.c_iflag & IXANY) {
				tty->inhibited = false;
			} else if(tty_is_cchar(tty, ch, VSTART)) {
				tty->inhibited = false;
				return STATUS_SUCCESS;
			}
		}
	}

	if(tty->inhibited) {
		return STATUS_SUCCESS;
	}

	/* Perform canonical-mode processing. */
	if(tty->termios.c_lflag & ICANON) {
		if(tty_is_cchar(tty, ch, VERASE)) {
			/* Erase one character. */
			if(!tty_buffer_erase(tty->input)) {
				return STATUS_SUCCESS;
			}

			/* ECHOE means print an erasing backspace. */
			if(tty->termios.c_lflag & ECHOE) {
				tty_echo(tty, '\b', true);
				tty_echo(tty, ' ', true);
				tty_echo(tty, '\b', true);
			} else {
				tty_echo(tty, ch, false);
			}

			return STATUS_SUCCESS;
		} else if(tty_is_cchar(tty, ch, VKILL)) {
			erase = tty_buffer_kill(tty->input);
			if(erase == 0) {
				return STATUS_SUCCESS;
			}

			if(tty->termios.c_lflag & ECHOE) {
				while(erase--) {
					tty_echo(tty, '\b', true);
					tty_echo(tty, ' ', true);
					tty_echo(tty, '\b', true);
				}
			}

			if(tty->termios.c_lflag & ECHOK) {
				tty_echo(tty, '\n', true);
			}

			return STATUS_SUCCESS;
		}
	}

	/* Generate signals on INTR and QUIT if ISIG is set. */
	if(tty->termios.c_lflag & ISIG) {
		//if(!tty->pgroup) {
		//	return STATUS_SUCCESS;
		//}

		if(tty_is_cchar(tty, ch, VINTR)) {
			/* TODO: Send signal. */
			return STATUS_SUCCESS;
		} else if(tty_is_cchar(tty, ch, VQUIT)) {
			/* TODO: Send signal. */
			return STATUS_SUCCESS;
		}
	}

	/* Mark stuff as newlines and put the character in the buffer. */
	if(ch == '\n' || tty_is_cchar(tty, ch, VEOF) || tty_is_cchar(tty, ch, VEOL)) {
		if(tty_is_cchar(tty, ch, VEOF)) {
			ch |= TTY_CHAR_EOF;
		}
		ch |= TTY_CHAR_NEWLINE;
	}

	/* Echo the character and insert it. */
	tty_echo(tty, ch, false);
	return tty_buffer_insert(tty->input, ch, nonblock);
}

/** Handle a TCSETA* request.
 * @todo		Handle action.
 * @param tty		Terminal request is being made on.
 * @param action	Action to perform.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @return		Status code describing result of operation. */
static status_t tty_request_setattr(tty_device_t *tty, int action, const void *in, size_t insz) {
	if(!in || insz != sizeof(tty->termios)) {
		return STATUS_INVALID_ARG;
	}

	memcpy(&tty->termios, in, sizeof(tty->termios));
	return STATUS_SUCCESS;
}

/** Handle a terminal request.
 * @param tty		Terminal request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of operation. */
static status_t tty_request(tty_device_t *tty, int request, const void *in, size_t insz,
                            void **outp, size_t *outszp) {
	status_t ret;
	int action;

	mutex_lock(&tty->lock);

	switch(request) {
	case TIOCDRAIN:
		/* tcdrain(int fd) - TODO. */
		ret = STATUS_SUCCESS;
		break;
	case TCXONC:
		/* tcflow(int fd, int action). */
		if(!in || insz != sizeof(action)) {
			ret = STATUS_INVALID_ARG;
			break;
		}

		action = *(const int *)in;
		switch(action) {
		case TCIOFF:
			tty_input(tty, tty->termios.c_cc[VSTOP], false);
			break;
		case TCION:
			tty_input(tty, tty->termios.c_cc[VSTART], false);
			break;
		case TCOOFF:
		case TCOON:
			ret = STATUS_NOT_IMPLEMENTED;
			break;
		default:
			ret = STATUS_INVALID_ARG;
			break;
		}

		ret = STATUS_SUCCESS;
		break;
	case TCFLSH:
		/* tcflush(int fd, int action) - TODO. */
		ret = STATUS_NOT_IMPLEMENTED;
		break;
	case TCGETA:
		/* tcgetattr(int fd, struct termios *tiop). */
		if(!outp || !outszp) {
			ret = STATUS_INVALID_ARG;
			break;
		}

		*outp = kmemdup(&tty->termios, sizeof(tty->termios), MM_SLEEP);
		*outszp = sizeof(tty->termios);
		ret = STATUS_SUCCESS;
		break;
	case TCSETA:
		/* tcsetattr(int fd, TCSANOW). */
		ret = tty_request_setattr(tty, TCSANOW, in, insz);
		break;
	case TCSETAW:
		/* tcsetattr(int fd, TCSADRAIN). */
		ret = tty_request_setattr(tty, TCSADRAIN, in, insz);
		break;
	case TCSETAF:
		/* tcsetattr(int fd, TCSAFLUSH). */
		ret = tty_request_setattr(tty, TCSAFLUSH, in, insz);
		break;
	case TIOCGPGRP:
		/* tcgetpgrp(int fd) - TODO. */
	case TIOCSPGRP:
		/* tcsetpgrp(int fd, pid_t pgid) - TODO. */
		ret = STATUS_NOT_IMPLEMENTED;
		break;
	case TIOCGWINSZ:
		if(!outp || !outszp) {
			ret = STATUS_INVALID_ARG;
			break;
		}

		*outp = kmemdup(&tty->winsize, sizeof(tty->winsize), MM_SLEEP);
		*outszp = sizeof(tty->winsize);
		ret = STATUS_SUCCESS;
		break;
	case TIOCSWINSZ:
		if(!in || insz != sizeof(tty->winsize)) {
			ret = STATUS_INVALID_ARG;
			break;
		}

		memcpy(&tty->winsize, in, sizeof(tty->winsize));
		ret = STATUS_SUCCESS;
		break;
	default:
		ret = STATUS_INVALID_REQUEST;
		break;
	};

	mutex_unlock(&tty->lock);
	return ret;
}

/** Destroy a terminal slave device.
 * @param device	Device to destroy. */
static void tty_slave_destroy(device_t *device) {
	tty_release(device->data);
}

/** Read from a terminal slave device.
 * @param device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t tty_slave_read(device_t *device, void *data, void *buf, size_t count,
                               offset_t offset, size_t *bytesp) {
	tty_device_t *tty = device->data;

	if(tty->termios.c_lflag & ICANON) {
		return tty_buffer_read_line(tty->input, buf, count, false, bytesp);
	} else {
		return tty_buffer_read(tty->input, buf, count, false, bytesp);
	}
}

/** Write to a terminal slave device.
 * @param device	Device to write to.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t tty_slave_write(device_t *device, void *data, const void *buf, size_t count,
                                offset_t offset, size_t *bytesp) {
	tty_device_t *tty = device->data;
	return pipe_write(tty->output, buf, count, false, bytesp);
}

/** Signal that a terminal slave event is being waited for.
 * @param device	Device to wait for.
 * @param data		Handle-specific data pointer (unused).
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer.
 * @return		Status code describing result of the operation. */
static status_t tty_slave_wait(device_t *device, void *data, int event, void *sync) {
	tty_device_t *tty = device->data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		if(tty->termios.c_lflag & ICANON) {
			if(semaphore_count(&tty->input->lines)) {
				object_wait_signal(sync);
			} else {
				notifier_register(&tty->input->lines_notifier, object_wait_notifier, sync);
			}
		} else {
			if(semaphore_count(&tty->input->data)) {
				object_wait_signal(sync);
			} else {
				notifier_register(&tty->input->data_notifier, object_wait_notifier, sync);
			}
		}
		return STATUS_SUCCESS;
	case DEVICE_EVENT_WRITABLE:
		pipe_wait(tty->output, true, sync);
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a terminal slave event.
 * @param device	Device to stop waiting for.
 * @param data		Handle-specific data pointer (unused).
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer. */
static void tty_slave_unwait(device_t *device, void *data, int event, void *sync) {
	tty_device_t *tty = device->data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		/* Remove from both in case ICANON was changed while waiting. */
		notifier_unregister(&tty->input->lines_notifier, object_wait_notifier, sync);
		notifier_unregister(&tty->input->data_notifier, object_wait_notifier, sync);
		break;
	case DEVICE_EVENT_WRITABLE:
		pipe_unwait(tty->output, true, sync);
		break;
	}
}

/** Handle a terminal slave request.
 * @param device	Device request is being made on.
 * @param data		Handle-specific data pointer (unused).
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of operation. */
static status_t tty_slave_request(device_t *device, void *data, int request, const void *in,
                                  size_t insz, void **outp, size_t *outszp) {
	tty_device_t *tty = device->data;
	return tty_request(tty, request, in, insz, outp, outszp);
}

/** Slave terminal device operations. */
static device_ops_t tty_slave_ops = {
	.destroy = tty_slave_destroy,
	.read = tty_slave_read,
	.write = tty_slave_write,
	.wait = tty_slave_wait,
	.unwait = tty_slave_unwait,
	.request = tty_slave_request,
};

/** Open the terminal master device.
 * @param device	Device being obtained.
 * @param datap		Where to store handle-specific data pointer.
 * @return		Status code describing result of the operation. */
static status_t tty_master_open(device_t *device, void **datap) {
	char name[DEVICE_NAME_MAX];
	tty_device_t *tty;
	status_t ret;

	/* Create a new terminal .*/
	tty = kmalloc(sizeof(tty_device_t), MM_SLEEP);
	mutex_init(&tty->lock, "tty_device_lock", 0);
	refcount_set(&tty->count, 2);
	tty->id = atomic_inc(&next_tty_id);
	tty->output = pipe_create();
	tty->input = tty_buffer_create();
	tty->escaped = false;
	tty->inhibited = false;
	memcpy(&tty->termios, &termios_defaults, sizeof(tty->termios));
	tty->winsize.ws_col = 80;
	tty->winsize.ws_row = 25;
	
	sprintf(name, "%d", tty->id); 
	ret = device_create(name, tty_device_dir, &tty_slave_ops, tty, NULL, 0, &tty->slave);
	if(ret != STATUS_SUCCESS) {
		pipe_destroy(tty->output);
		tty_buffer_destroy(tty->input);
		kfree(tty);
		return ret;
	}

	*datap = tty;
	return STATUS_SUCCESS;
}

/** Close the terminal master device.
 * @param device	Device being closed.
 * @param data		Pointer to terminal structure. */
static void tty_master_close(device_t *device, void *data) {
	tty_device_t *tty = data;

	/* FIXME: Device manager doesn't allow removal of in-use devices yet. */
	device_destroy(tty->slave);
	tty_release(tty);
}

/** Read from the terminal master device.
 * @param device	Device to read from.
 * @param data		Pointer to terminal structure.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t tty_master_read(device_t *device, void *data, void *buf, size_t count,
                                offset_t offset, size_t *bytesp) {
	tty_device_t *tty = data;
	return pipe_read(tty->output, buf, count, false, bytesp);
}

/** Write to the terminal master device.
 * @param device	Device to write to.
 * @param data		Pointer to terminal structure.
 * @param _buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t tty_master_write(device_t *device, void *data, const void *_buf, size_t count,
                                 offset_t offset, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	tty_device_t *tty = data;
	const char *buf = _buf;
	size_t i;

	mutex_lock(&tty->lock);

	for(i = 0; i < count; i++) {
		ret = tty_input(tty, buf[i], false);
		if(ret != STATUS_SUCCESS) {
			break;
		}
	}

	mutex_unlock(&tty->lock);
	*bytesp = i;
	return ret;
}

/** Signal that a terminal master event is being waited for.
 * @param device	Device to wait for.
 * @param data		Pointer to terminal structure.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer.
 * @return		Status code describing result of the operation. */
static status_t tty_master_wait(device_t *device, void *data, int event, void *sync) {
	tty_device_t *tty = data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_wait(tty->output, false, sync);
		return STATUS_SUCCESS;
	case DEVICE_EVENT_WRITABLE:
		if(semaphore_count(&tty->input->space)) {
			object_wait_signal(sync);
		} else {
			notifier_register(&tty->input->space_notifier, object_wait_notifier, sync);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a terminal master event.
 * @param device	Device to stop waiting for.
 * @param data		Pointer to terminal structure.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer. */
static void tty_master_unwait(device_t *device, void *data, int event, void *sync) {
	tty_device_t *tty = data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_unwait(tty->output, false, sync);
		break;
	case DEVICE_EVENT_WRITABLE:
		notifier_unregister(&tty->input->space_notifier, object_wait_notifier, sync);
		break;
	}
}

/** Handler for terminal master requests.
 * @param device	Device request is being made on.
 * @param data		Pointer to terminal structure.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t tty_master_request(device_t *device, void *data, int request, const void *in,
                                   size_t insz, void **outp, size_t *outszp) {
	tty_device_t *tty = data;

	switch(request) {
	case TTY_MASTER_ID:
		if(!outp || !outszp) {
			return STATUS_INVALID_ARG;
		}

		*outp = kmemdup(&tty->id, sizeof(tty->id), MM_SLEEP);
		*outszp = sizeof(tty->id);
		return STATUS_SUCCESS;
	default:
		return tty_request(tty, request, in, insz, outp, outszp);
	}
}

/** Terminal master device operations. */
static device_ops_t tty_master_ops = {
	.open = tty_master_open,
	.close = tty_master_close,
	.read = tty_master_read,
	.write = tty_master_write,
	.wait = tty_master_wait,
	.unwait = tty_master_unwait,
	.request = tty_master_request,
};

/** Initialisation function for the terminal module.
 * @return		Status code describing result of the operation. */
static status_t tty_init(void) {
	status_t ret;

	/* Create terminal device directory. */
	ret = device_create("tty", device_tree_root, NULL, NULL, NULL, 0, &tty_device_dir);	
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Create master device. */
	ret = device_create("master", tty_device_dir, &tty_master_ops, NULL, NULL,
	                    0, &tty_master_device);
	if(ret != STATUS_SUCCESS) {
		device_destroy(tty_device_dir);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Unloading function for the terminal module.
 * @return		Status code describing result of the operation. */
static status_t tty_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("tty");
MODULE_DESC("POSIX terminal device manager");
MODULE_FUNCS(tty_init, tty_unload);
