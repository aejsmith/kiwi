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

#include <kiwi/Graphics/Point.h>
#include <kiwi/Support/Noncopyable.h>

class Session;
class ServerWindow;

/** Class implementing a cursor. */
class Cursor : kiwi::Noncopyable {
public:
	Cursor(Session *session);
	~Cursor();

	void SetVisible(bool visible);
	void MoveRelative(int32_t dx, int32_t dy);
	kiwi::Point GetPosition() const;
private:
	Session *m_session;		/**< Session the cursor is for. */
	ServerWindow *m_root;		/**< Root window of session, stored for convenience. */
	ServerWindow *m_window;		/**< Window implementing the cursor. */
};

#endif /* __CURSOR_H */
