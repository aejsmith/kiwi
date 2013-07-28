/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
