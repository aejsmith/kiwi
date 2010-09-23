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
 * @brief		Window list class.
 */

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>

#include "Window.h"
#include "WindowList.h"

using namespace std;

/** Construct the window list. */
WindowList::WindowList() {};

/** Add a window to the window list.
 * @param window	Window to add. */
void WindowList::AddWindow(Window *window) {
	ListForWindow(window).push_back(window);
	RebuildList();
}

/** Remove a window from the list.
 * @param window	Window to remove. */
void WindowList::RemoveWindow(Window *window) {
	ListForWindow(window).remove(window);
	m_list.remove(window);
}

/** Get the list a window should be placed in.
 * @param window	Window.
 * @return		Reference to list. */
WindowList::List &WindowList::ListForWindow(Window *window) {
	switch(window->GetType()) {
	case WINDOW_TYPE_NORMAL:
	case WINDOW_TYPE_UNBORDERED:
	case WINDOW_TYPE_ALERT:
	case WINDOW_TYPE_CHILD:
		return m_normal;
	case WINDOW_TYPE_PANEL:
		return m_panels;
	case WINDOW_TYPE_POPUP:
		return m_popups;
	case WINDOW_TYPE_ROOT:
		clog << "Root window should not be placed in WindowList" << endl;
		abort();
	default:
		clog << "Invalid window type, should be validated elsewhere." << endl;
		abort();
	}
}

/** Rebuild the list of all windows. */
void WindowList::RebuildList() {
	m_list.clear();
	copy(m_normal.begin(), m_normal.end(), back_inserter(m_list));
	copy(m_panels.begin(), m_panels.end(), back_inserter(m_list));
	copy(m_popups.begin(), m_popups.end(), back_inserter(m_list));
}
