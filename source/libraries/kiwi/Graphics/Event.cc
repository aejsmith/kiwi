/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Graphics event classes.
 */

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Graphics/Event.h>

using namespace kiwi;

/** Define a function to check if a flag was set in the change. */
#define CHECK_SET(name, flag)		\
	bool WindowStateEvent::name () const { \
		return (!(m_prev & flag) && ((m_state & flag) == flag)); \
	}

/** Define a function to check if a flag was cleared in the change. */
#define CHECK_CLEARED(name, flag)		\
	bool WindowStateEvent::name () const { \
		return (((m_prev & flag) == flag) && !(m_state & flag)); \
	}

CHECK_SET(WasShown, BaseWindow::kVisibleState);
CHECK_CLEARED(WasHidden, BaseWindow::kVisibleState);
CHECK_SET(WasActivated, BaseWindow::kActiveState);
CHECK_CLEARED(WasDeactivated, BaseWindow::kActiveState);
CHECK_SET(WasMaximized, BaseWindow::kMaximizedState);
CHECK_CLEARED(WasUnmaximized, BaseWindow::kMaximizedState);
CHECK_SET(WasMinimized, BaseWindow::kMinimizedState);
CHECK_CLEARED(WasUnminimized, BaseWindow::kMinimizedState);
