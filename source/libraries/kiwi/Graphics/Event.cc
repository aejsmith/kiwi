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
