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
 * @brief		Application class.
 */

#ifndef __KIWI_APPLICATION_H
#define __KIWI_APPLICATION_H

#include <kiwi/EventLoop.h>

namespace kiwi {

/** IPC server class. */
class Application : public EventLoop {
public:
	Application();
	~Application();
private:
	void *m_conn;			/**< Connection to the application server. */
};

}

#endif /* __KIWI_APPLICATION_H */
