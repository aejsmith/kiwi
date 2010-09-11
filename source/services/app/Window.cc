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
 * @brief		Window class.
 *
 * The server does not implement the displaying of windows, or any policy on
 * things like window order, position, focus, etc. This is all left up to the
 * window manager. All that the server does is provide the mechanism for
 * creation and deletion of windows, store properties about windows and provide
 * surfaces for window content.
 */

#include "Window.h"
