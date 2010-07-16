/*
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
	service = new Service("console", "Service providing a graphical console.", "/system/services/console");
	svcmgr.addService(service);

	service = new Service("pong", "Service that pongs pings.", "/system/services/pong", Service::OnDemand);
	service->addPort("org.kiwi.Pong");
	svcmgr.addService(service);

	service = new Service("shmserver", "Shared memory test server.", "/system/services/shmserver", Service::OnDemand);
	service->addPort("org.kiwi.SHMServer");
	svcmgr.addService(service);

	service = new Service("kittenserver", "Kitten server.", "/system/services/kittenserver", Service::OnDemand);
	service->addPort("org.kiwi.KittenServer");
	svcmgr.addService(service);

	svcmgr.run();
	return 0;
}
