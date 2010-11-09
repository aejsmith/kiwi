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
#include <iterator>

#include "ServerWindow.h"
#include "WindowList.h"

using namespace std;

WindowList::Iterator::Iterator(const Map &map, Map::const_iterator iter) :
	m_map(map), m_map_iter(iter)
{
	if(m_map_iter != m_map.end()) {
		m_list_iter = m_map_iter->second.begin();
	}
}

WindowList::Iterator &WindowList::Iterator::operator ++() {
	if(m_map_iter != m_map.end()) {
		if(++m_list_iter == m_map_iter->second.end()) {
			if(++m_map_iter != m_map.end()) {
				m_list_iter = m_map_iter->second.begin();
			} else {
				m_list_iter = List::iterator();
			}
		}
	}
	return *this;
}

WindowList::Iterator &WindowList::Iterator::operator --() {
	if(m_map_iter == m_map.end() || m_list_iter-- == m_map_iter->second.begin()) {
		if(m_map_iter-- != m_map.begin()) {
			m_list_iter = --m_map_iter->second.end();
		} else {
			m_list_iter = List::iterator();
		}
	}

	return *this;
}

bool WindowList::Iterator::operator ==(const Iterator &other) {
	if(m_map == other.m_map && m_map_iter == other.m_map_iter) {
		if(m_map_iter == m_map.end() || m_list_iter == other.m_list_iter) {
			return true;
		}
	}
	return false;
}

bool WindowList::Iterator::operator !=(const Iterator &other) {
	return !(*this == other);
}

/** Construct the window list. */
WindowList::WindowList() {};

/** Add a window to the window list.
 * @param window	Window to add. */
void WindowList::Insert(ServerWindow *window) {
	ListForWindow(window).push_back(window);
}

/** Remove a window from the list.
 * @param window	Window to remove. */
void WindowList::Remove(ServerWindow *window) {
	List &list = ListForWindow(window);
	list.remove(window);
	if(list.empty()) {
		m_windows.erase(window->GetLevel());
	}
}

/** Move a window above all others in its level.
 * @param window	Window to move forward.
 * @return		Whether the list position changed. */
bool WindowList::MoveToFront(ServerWindow *window) {
	List &list = ListForWindow(window);
	if(list.empty() || *(--list.end()) != window) {
		list.remove(window);
		list.push_back(window);
		return true;
	}

	return false;
}

/** Get the list containing a window.
 * @param window	Window to get list for.
 * @return		Reference to list for the window. */
WindowList::List &WindowList::ListForWindow(ServerWindow *window) {
	Map::iterator it = m_windows.find(window->GetLevel());
	if(it == m_windows.end()) {
		it = m_windows.insert(make_pair(window->GetLevel(), List())).first;
	}

	return it->second;
}
