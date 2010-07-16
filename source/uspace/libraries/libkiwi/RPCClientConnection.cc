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
 * @brief		RPC client connection class.
 *
 * @todo		Should this handle errors somehow? I'm not going to
 *			use exceptions here.
 */

#include <kiwi/RPC.h>

using namespace kiwi;

/** Constructor for a client connection.
 * @param name		Name of the service.
 * @param version	Version of the service.
 * @param handle	Handle to the connection. */
RPCClientConnection::RPCClientConnection(const char *name, uint32_t version, handle_t handle) :
	m_conn(handle), m_name(name), m_version(version)
{
	/* Hook up the signal handlers. */
	m_conn.onMessage.connect(this, &RPCClientConnection::_handleMessage);
	m_conn.onHangup.connect(this, &RPCClientConnection::_handleHangup);

	/* Send the version to the client. */
	RPCMessageBuffer buf;
	buf << std::string(m_name);
	buf << version;
	sendEvent(0, buf);
}

/** Send an event to the client.
 * @param id		ID of the event.
 * @param buf		Message buffer. */
void RPCClientConnection::sendEvent(uint32_t id, RPCMessageBuffer &buf) {
	m_conn.send(id, buf.getBuffer(), buf.getSize());
}

/** Handle the connection being hung up.
 * @note		The default version of this function does nothing.
 *			Ideally the real implementation should clean up and
 *			delete the connection. */
void RPCClientConnection::handleHangup() {
	/* Nothing happens. */
}

/** Signal handler for a message being received.
 * @param conn		Connection the message was received on. */
void RPCClientConnection::_handleMessage(IPCConnection *conn) {
	uint32_t id;
	size_t size;
	char *data;
	if(!m_conn.receive(id, data, size)) {
		return;
	}

	RPCMessageBuffer buf(data, size);
	handleMessage(id, buf);
}

/** Signal handler for the remote end being hung up.
 * @fixme		Get rid of this, connect to handleHangup() directly.
 * @param conn		Connection that was hung up. */
void RPCClientConnection::_handleHangup(IPCConnection *conn) {
	handleHangup();
}
