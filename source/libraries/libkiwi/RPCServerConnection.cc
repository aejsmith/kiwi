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
 * @brief		RPC message buffer class.
 */

#include <kiwi/RPC.h>

#include <sstream>
#include <stdexcept>

using namespace kiwi;

/** Construct an RPC server connection object.
 * @param name		Name of the service.
 * @param version	Service version.
 * @param id		If not negative, this port will be used rather than
 *			looking up the port name. */
RPCServerConnection::RPCServerConnection(const char *name, uint32_t version, port_id_t id) :
	m_name(name), m_version(version)
{
	m_conn.OnMessage.Connect(this, &RPCServerConnection::HandleMessage);

	/* Connect to the server. */
	if(id >= 0) {
		m_conn.Connect(id);
	} else {
		m_conn.Connect(m_name);
	}

	/* Check the server version. */
	CheckVersion();
}

/** Send a message on the connection and get the response.
 * @param id		ID of message to send.
 * @param buf		Buffer containing message to send. Will be replaced
 *			with the response message. */
void RPCServerConnection::SendMessage(uint32_t id, RPCMessageBuffer &buf) {
	m_conn.Send(id, buf.GetBuffer(), buf.GetSize());

	/* The server may send us events before we get the actual reply. If
	 * the ID is not what is expected, pass it to the event handler. */
	while(true) {
		uint32_t nid;
		ReceiveMessage(nid, buf);
		if(nid == id) {
			return;
		} else {
			HandleEvent(nid, buf);
		}
	}
}

/** Receive a message on the connection.
 * @param id		Where to store ID of message.
 * @param buf		Where to store message buffer. */
void RPCServerConnection::ReceiveMessage(uint32_t &id, RPCMessageBuffer &buf) {
	size_t size;
	char *data;
	m_conn.Receive(id, data, size);
	buf.Reset(data, size);
}

/** Handle a message on the connection. */
void RPCServerConnection::HandleMessage() {
	RPCMessageBuffer buf;
	uint32_t id;
	ReceiveMessage(id, buf);
	HandleEvent(id, buf);
}

/** Check whether the server is the expected version and throw an exception if not. */
void RPCServerConnection::CheckVersion() {
	RPCMessageBuffer buf;
	uint32_t version, id;
	std::string name;

	/* The server should send us a message containing the service
	 * name followed by the version when we open the connection. */
	ReceiveMessage(id, buf);
	if(id != 0) {
		throw RPCError("Server did not send version message");
	}
	buf >> name;
	buf >> version;
	if(name != m_name) {
		std::stringstream msg;
		msg << "Server's service name is incorrect (wanted " << m_name;
		msg << ", got " << name;
		throw RPCError(msg.str());
	} else if(version != m_version) {
		std::stringstream msg;
		msg << "Client/server version mismatch (wanted " << m_version;
		msg << ", got " << version;
		throw RPCError(msg.str());
	}
}
