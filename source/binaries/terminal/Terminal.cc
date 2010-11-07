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
#include <kiwi/Process.h>

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
Terminal::Terminal(int cols, int rows) :
	m_id(0), m_columns(cols), m_rows(rows), m_cursor_x(0), m_cursor_y(0)
{
	handle_t handle;
	status_t ret;

	m_process.OnExit.Connect(OnExit);

	/* Open the terminal master. */
	ret = device_open("/tty/master", DEVICE_READ | DEVICE_WRITE, &handle);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to create new terminal: " << e.GetDescription() << endl;
		throw e;
	}
	SetHandle(handle);

	/* Get the ID of the slave. */
	ret = device_request(handle, TTY_MASTER_ID, 0, 0, &m_id, sizeof(m_id), 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to get terminal slave ID: " << e.GetDescription() << endl;
		throw e;
	}

	/* Set the window size. */
	winsize size;
	size.ws_col = cols;
	size.ws_row = rows;
	ret = device_request(handle, TIOCSWINSZ, &size, sizeof(size), 0, 0, 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to set terminal size: " << e.GetDescription() << endl;
		throw e;
	}

	/* Create our buffer. */
	m_buffer = new char *[rows];
	for(int i = 0; i < rows; i++) {
		m_buffer[i] = new char[cols];
		memset(m_buffer[i], 0, cols);
	}
}

/** Destroy the terminal. */
Terminal::~Terminal() {

}

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
	ret = device_open(path.str().c_str(), DEVICE_READ, &in);
	if(ret != STATUS_SUCCESS) {
		return false;
	}
	ret = device_open(path.str().c_str(), DEVICE_WRITE, &out);
	if(ret != STATUS_SUCCESS) {
		handle_close(in);
		return false;
	}
	ret = device_open(path.str().c_str(), DEVICE_WRITE, &err);
	if(ret != STATUS_SUCCESS) {
		handle_close(out);
		handle_close(in);
		return false;
	}

	/* Make the handles inheritable so children of the process get them. */
	handle_set_flags(in, HANDLE_INHERITABLE);
	handle_set_flags(out, HANDLE_INHERITABLE);
	handle_set_flags(err, HANDLE_INHERITABLE);

	/* Create the child process. */
	Process::HandleMap map;
	map.push_back(make_pair(in, 0));
	map.push_back(make_pair(out, 1));
	map.push_back(make_pair(err, 2));
	ret = m_process.Create(cmdline, environ, &map);
	handle_close(err);
	handle_close(out);
	handle_close(in);
	return ret;
}

/** Resize the terminal.
 * @param cols		New number of columns.
 * @param rows		New number of rows. */
void Terminal::Resize(int cols, int rows) {
	if(cols == m_columns && rows == m_rows) {
		return;
	}

	winsize size;
	size.ws_col = cols;
	size.ws_row = rows;

	status_t ret = device_request(m_handle, TIOCSWINSZ, &size, sizeof(size), 0, 0, 0);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to resize terminal: " << e.GetDescription() << endl;
		return;
	}

	/* TODO. */
}

/** Send input to the terminal.
 * @param ch		Character to send. */
void Terminal::Input(unsigned char ch) {
	status_t ret = device_write(m_handle, &ch, 1, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		cout << "Failed to send input to terminal: " << e.GetDescription() << endl;
		return;
	}
}

/** Add output to the terminal.
 * @param ch		Character to add. */
void Terminal::Output(unsigned char ch) {
	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(m_cursor_x) {
			m_cursor_x--;
		} else if(m_cursor_y) {
			m_cursor_x = m_columns - 1;
			m_cursor_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		m_cursor_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		m_cursor_x = 0;
		m_cursor_y++;
		break;
	case '\t':
		m_cursor_x += 8 - (m_cursor_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		m_buffer[m_cursor_y][m_cursor_x] = ch;
		OnUpdate(Rect(m_cursor_x, m_cursor_y, 1, 1));
		m_cursor_x++;
		break;
	}

	/* If we have reached the edge of the console insert a new line. */
	if(m_cursor_x >= m_columns) {
		m_cursor_x = 0;
		m_cursor_y++;
	}

	/* If we have reached the bottom of the console, scroll. */
	if(m_cursor_y >= m_rows) {
		ScrollDown();
		m_cursor_y = m_rows - 1;
	}
}

/** Scroll the terminal down. */
void Terminal::ScrollDown() {
	/* Delete the top row of the buffer. */
	delete m_buffer[0];

	/* Shift everything up. */
	memmove(&m_buffer[0], &m_buffer[1], m_rows * sizeof(m_buffer[0]));

	/* Add a new row. */
	m_buffer[m_rows - 1] = new char[m_columns];
	memset(m_buffer[m_rows - 1], 0, m_columns);

	/* Update. */
	OnUpdate(Rect(0, 0, m_columns, m_rows));
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

	ret = device_read(m_handle, &ch, 1, 0, &bytes);
	if(ret != STATUS_SUCCESS || bytes != 1) {
		return;
	}

	Output(ch);
}
