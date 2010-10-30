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
 */

#include <kiwi/RPC.h>
#include "Internal.h"

using namespace kiwi;

/** Constructor for a client connection.
 * @param name		Name of the service.
 * @param version	Version of the service.
 * @param handle	Handle to the connection. */
RPCClientConnection::RPCClientConnection(const char *name, uint32_t version, handle_t handle) :
	m_conn(handle), m_name(name), m_version(version)
{
	/* Hook up the signal handlers. */
	m_conn.OnMessage.Connect(this, &RPCClientConnection::_HandleMessage);
	m_conn.OnHangup.Connect(this, &RPCClientConnection::HandleHangup);

	/* Send the version to the client. */
	RPCMessageBuffer buf;
	buf << std::string(m_name);
	buf << version;
	SendMessage(0, buf);
}

/** Send a message to the client.
 * @param id		ID of the message.
 * @param buf		Message buffer. */
void RPCClientConnection::SendMessage(uint32_t id, RPCMessageBuffer &buf) {
	if(!m_conn.Send(id, buf.GetBuffer(), buf.GetSize())) {
		libkiwi_debug("Failed to send message to client: %s", m_conn.GetError().GetDescription());
	}
}

/** Handle the connection being hung up.
 * @note		The default version of this function calls DeleteLater()
 *			on the connection. */
void RPCClientConnection::HandleHangup() {
	DeleteLater();
}

/** Signal handler for a message being received. */
void RPCClientConnection::_HandleMessage() {
	try {
		uint32_t id;
		size_t size;
		char *data;
		if(!m_conn.Receive(id, data, size)) {
			libkiwi_debug("Failed to receive message from client: %s",
			              m_conn.GetError().GetDescription());
			return;
		}

		RPCMessageBuffer buf(data, size);
		HandleMessage(id, buf);
		SendMessage(id, buf);
	} catch(RPCError &e) {
		libkiwi_warn("RPC error during message handling: %s", e.GetDescription());
		return;
	}
}
