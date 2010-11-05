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

#include <list>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace kiwi;
using namespace std;

/** Construct an RPC server connection object.
 * @param name		Name of the service.
 * @param version	Service version.
 * @param handle	If not negative, an existing connection handle to the
 *			server. It is expected that this connection is newly
 *			set up, i.e. there is a version message waiting. */
RPCServerConnection::RPCServerConnection(const char *name, uint32_t version, handle_t handle) :
	m_conn(handle), m_name(name), m_version(version)
{
	m_conn.OnMessage.Connect(this, &RPCServerConnection::HandleMessage);

	/* If given a handle, check the version. */
	if(handle >= 0) {
		CheckVersion();
	}
}

/** Connect to the server.
 * @param id		A port ID to connect to. If negative (the default), the
 *			service's name will be looked up and connected to. */
void RPCServerConnection::Connect(port_id_t id) {
	if(id >= 0) {
		if(!m_conn.Connect(id)) {
			throw Error(m_conn.GetError());
		}
	} else {
		if(!m_conn.Connect(m_name)) {
			throw Error(m_conn.GetError());
		}
	}

	/* Check the server version. */
	CheckVersion();
}

/** Send a message on the connection and get the response.
 * @param id		ID of message to send.
 * @param buf		Buffer containing message to send. Will be replaced
 *			with the response message. */
void RPCServerConnection::SendMessage(uint32_t id, RPCMessageBuffer &buf) {
	if(!m_conn.Send(id, buf.GetBuffer(), buf.GetSize())) {
		throw Error(m_conn.GetError());
	}

	/* The server may send us events before we get the actual reply. If
	 * the ID is not what is expected, store them all until we get the
	 * reply we want, then handle them. */
	list<pair<uint32_t, RPCMessageBuffer> > events;
	while(true) {
		uint32_t nid;
		ReceiveMessage(nid, buf);
		if(nid == id) {
			break;
		} else {
			events.push_back(make_pair(nid, std::move(buf)));
		}
	}

	/* Handle the received events. */
	for(auto it = events.begin(); it != events.end(); ++it) {
		HandleEvent(it->first, it->second);
	}
}

/** Receive a message on the connection.
 * @param id		Where to store ID of message.
 * @param buf		Where to store message buffer. */
void RPCServerConnection::ReceiveMessage(uint32_t &id, RPCMessageBuffer &buf) {
	size_t size;
	char *data;

	if(!m_conn.Receive(id, data, size)) {
		throw Error(m_conn.GetError());
	}

	buf.Reset(data, size);
}

/** Handle an event on the connection.
 * @param id		Message ID.
 * @param buf		Message buffer. */
void RPCServerConnection::HandleEvent(uint32_t id, RPCMessageBuffer &buf) {
	/* The default implementation recognises no events. */
	ostringstream msg;
	msg << "Received unknown event ID: " << id;
	throw RPCError(msg.str());
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
	string name;

	/* The server should send us a message containing the service
	 * name followed by the version when we open the connection. */
	ReceiveMessage(id, buf);
	if(id != 0) {
		throw RPCError("Server did not send version message");
	}
	buf >> name;
	buf >> version;
	if(name != m_name) {
		ostringstream msg;
		msg << "Server's service name is incorrect (wanted " << m_name;
		msg << ", got " << name;
		throw RPCError(msg.str());
	} else if(version != m_version) {
		ostringstream msg;
		msg << "Client/server version mismatch (wanted " << m_version;
		msg << ", got " << version;
		throw RPCError(msg.str());
	}
}
