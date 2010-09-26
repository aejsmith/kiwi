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

#ifndef __WINDOWLIST_H
#define __WINDOWLIST_H

#include <list>

class Window;

/** Class managing the order of windows. */
class WindowList {
public:
	/** Internal list type. */
	typedef std::list<Window *> List;

	WindowList();

	void Insert(Window *window);
	void Remove(Window *window);
	bool MoveToFront(Window *window);

	/** Get the list head.
	 * @return		Iterator to first window in the list. */
	List::const_iterator Begin() { return m_list.begin(); }

	/** Get the list end.
	 * @return		Iterator pointing after last window in the list. */
	List::const_iterator End() { return m_list.end(); }

	/** Get the reverse list head.
	 * @return		Iterator to last window in the list. */
	List::const_reverse_iterator RBegin() { return m_list.rbegin(); }

	/** Get the reverse list end.
	 * @return		Iterator pointing after first window in the list. */
	List::const_reverse_iterator REnd() { return m_list.rend(); }
private:
	List &ListForWindow(Window *window);
	void RebuildList();

	List m_list;			/**< List of windows. */
	List m_normal;			/**< List of normal windows. */
	List m_panels;			/**< List of panel windows. */
	List m_popups;			/**< List of popup windows. */
	List m_cursors;			/**< List of cursor windows. */
};

#endif /* __WINDOWLIST_H */
