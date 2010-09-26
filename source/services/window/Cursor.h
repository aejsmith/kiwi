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
 * @brief		Cursor class.
 */

#ifndef __CURSOR_H
#define __CURSOR_H

#include <kiwi/Object.h>

class Window;

/** Class implementing a cursor. */
class Cursor : public kiwi::Object {
public:
	Cursor(Window *root);
	~Cursor();

	void SetVisible(bool visible);
	void MoveRelative(int dx, int dy);
private:
	Window *m_root;			/**< Root window. */
	Window *m_window;		/**< Window implementing the cursor. */
};

#endif /* __CURSOR_H */
