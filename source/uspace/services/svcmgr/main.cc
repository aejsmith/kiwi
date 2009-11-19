/* Kiwi service manager
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Service manager.
 */

#include "Service.h"
#include "ServiceManager.h"

int main(int argc, char **argv) {
	ServiceManager svcmgr;
	Service *service;

	/* Add services. TODO: These should be in configuration files. */
	service = new Service("console", "Service providing a graphical console.", "/system/binaries/console");
	svcmgr.AddService(service);

	service = new Service("pong", "Service that pongs pings.", "/system/binaries/pong", Service::OnDemand);
	service->AddPort("org.kiwi.Pong");
	svcmgr.AddService(service);

	svcmgr.Run();
	return 0;
}
