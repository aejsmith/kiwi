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
 * @brief		Terminal class.
 */

#include <kernel/device.h>
#include <kernel/status.h>

#include <kiwi/Error.h>

#include <assert.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "Terminal.h"

using namespace kiwi;
using namespace std;

/** Create a new terminal.
 * @param cols		Number of columns.
 * @param rows		Number of rows. */
Terminal::Terminal(Handler *handler, int cols, int rows) :
	m_id(0), m_handler(handler), m_cols(cols), m_rows(rows)
{
	handle_t handle;
	status_t ret;

	m_process.OnExit.Connect(OnExit);

	/* Open the terminal master. */
	ret = kern_device_open("/tty/master", DEVICE_READ | DEVICE_WRITE, &handle);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to create new terminal: " << e.GetDescription() << endl;
		throw e;
	}
	SetHandle(handle);

	/* Get the ID of the slave. */
	ret = kern_device_request(handle, TTY_MASTER_ID, 0, 0, &m_id, sizeof(m_id), 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to get terminal slave ID: " << e.GetDescription() << endl;
		throw e;
	}

	/* Set the window size. */
	winsize size;
	size.ws_col = cols;
	size.ws_row = rows;
	ret = kern_device_request(handle, TIOCSWINSZ, &size, sizeof(size), 0, 0, 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to set terminal size: " << e.GetDescription() << endl;
		throw e;
	}

	m_handler->Resize(cols, rows);
}

/** Destroy the terminal. */
Terminal::~Terminal() {}

/** Run a command in the terminal.
 * @param cmdline	Command line to run. */
bool Terminal::Run(const char *cmdline) {
	handle_t in, out, err;
	status_t ret;

	assert(!m_process.IsRunning());

	/* Work out the path to the slave. */
	ostringstream path;
	path << "/tty/" << m_id;

	/* Open handles to it to give to the child. */
	ret = kern_device_open(path.str().c_str(), DEVICE_READ, &in);
	if(ret != STATUS_SUCCESS) {
		return false;
	}
	ret = kern_device_open(path.str().c_str(), DEVICE_WRITE, &out);
	if(ret != STATUS_SUCCESS) {
		kern_handle_close(in);
		return false;
	}
	ret = kern_device_open(path.str().c_str(), DEVICE_WRITE, &err);
	if(ret != STATUS_SUCCESS) {
		kern_handle_close(out);
		kern_handle_close(in);
		return false;
	}

	/* Make the handles inheritable so children of the process get them. */
	kern_handle_control(in, HANDLE_SET_LFLAGS, HANDLE_INHERITABLE, NULL);
	kern_handle_control(out, HANDLE_SET_LFLAGS, HANDLE_INHERITABLE, NULL);
	kern_handle_control(err, HANDLE_SET_LFLAGS, HANDLE_INHERITABLE, NULL);

	/* Create the child process. */
	Process::HandleMap map;
	map.push_back(make_pair(in, 0));
	map.push_back(make_pair(out, 1));
	map.push_back(make_pair(err, 2));
	ret = m_process.Create(cmdline, environ, &map);
	kern_handle_close(err);
	kern_handle_close(out);
	kern_handle_close(in);
	return ret;
}

/** Resize the terminal.
 * @param cols		New number of columns.
 * @param rows		New number of rows. */
void Terminal::Resize(int cols, int rows) {
	status_t ret;

	if(cols == m_cols && rows == m_rows) {
		return;
	}

	/* Set the new window size of the terminal device. */
	winsize size;
	size.ws_col = cols;
	size.ws_row = rows;
	ret = kern_device_request(m_handle, TIOCSWINSZ, &size, sizeof(size), 0, 0, 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to resize terminal: " << e.GetDescription() << endl;
		return;
	}

	m_handler->Resize(cols, rows);
}

/** Send input to the terminal.
 * @param ch		Character to send. */
void Terminal::Input(unsigned char ch) {
	status_t ret = kern_device_write(m_handle, &ch, 1, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to send input to terminal: " << e.GetDescription() << endl;
		return;
	}
}

/** Add output to the terminal.
 * @param ch		Character to add. */
void Terminal::Output(unsigned char ch) {
	m_handler->Output(ch);
}

/** Register events for the terminal. */
void Terminal::RegisterEvents() {
	RegisterEvent(DEVICE_EVENT_READABLE);
}

/** Handle an event on the terminal handle.
 * @param event		Event that was received. */
void Terminal::HandleEvent(int event) {
	unsigned char ch;
	status_t ret;
	size_t bytes;

	assert(event == DEVICE_EVENT_READABLE);

	ret = kern_device_read(m_handle, &ch, 1, 0, &bytes);
	if(ret != STATUS_SUCCESS || bytes != 1) {
		return;
	}

	m_handler->Output(ch);
}
