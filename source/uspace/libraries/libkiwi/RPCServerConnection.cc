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
#include <stdexcept>

using namespace kiwi;

RPCServerConnection::RPCServerConnection(const char *name, uint32_t version) :
	m_name(name), m_version(version)
{
	m_conn.onMessage.connect(this, &RPCServerConnection::_handleMessage);
}

/** Connect to the server by the default name.
 * @return		Whether successfully connected. */
bool RPCServerConnection::connect() {
	return connect(m_name);
}

/** Connect to the server under a different name.
 * @param name		Name to connect to.
 * @return		Whether successfully connected. */
bool RPCServerConnection::connect(const char *name) {
	if(!m_conn.connect(name)) {
		return false;
	} else if(!checkVersion()) {
		m_conn.close();
		return false;
	}
	return true;
}

/** Connect to the server on the specified port.
 * @param port		Port ID to connect to.
 * @return		Whether successfully connected. */
bool RPCServerConnection::connect(port_id_t port) {
	if(!m_conn.connect(port)) {
		return false;
	} else if(!checkVersion()) {
		m_conn.close();
		return false;
	}
	return true;
}

/** Send a message on the connection and get the response.
 * @param id		ID of message to send.
 * @param buf		Buffer containing message to send. Will be replaced
 *			with the response message. */
void RPCServerConnection::sendMessage(uint32_t id, RPCMessageBuffer &buf) {
	if(!m_conn.send(id, buf.getBuffer(), buf.getSize())) {
		throw std::runtime_error("Failed to send message");
	}

	/* The server may send us events before we get the actual reply. If
	 * the message ID is not what is expected, pass it to the event handler */
	while(true) {
		uint32_t nid;
		receiveMessage(nid, buf);
		if(nid == id) {
			return;
		} else {
			handleEvent(nid, buf);
		}
	}
}

/** Receive a message on the connection.
 * @param id		Where to store ID of message.
 * @param buf		Where to store message buffer. */
void RPCServerConnection::receiveMessage(uint32_t &id, RPCMessageBuffer &buf) {
	size_t size;
	char *data;
	if(!m_conn.receive(id, data, size)) {
		throw std::runtime_error("Failed to receive message");
	}

	buf.reset(data, size);
}

/** Handle a message on the connection.
 * @param conn		Connection message was received on. */
void RPCServerConnection::_handleMessage(IPCConnection *conn) {
	RPCMessageBuffer buf;
	uint32_t id;
	receiveMessage(id, buf);
	handleEvent(id, buf);
}

/** Check whether the server is the expected version.
 * @return		Whether server is correct version. */
bool RPCServerConnection::checkVersion() {
	try {
		RPCMessageBuffer buf;
		uint32_t version, id;
		std::string name;

		/* The server should send us a message containing the service
		 * name followed by the version when we open the connection. */
		receiveMessage(id, buf);
		if(id != 0) {
			return false;
		}
		buf >> name;
		buf >> version;
		if(name != m_name || version != m_version) {
			return false;
		}
	} catch(std::runtime_error) {
		return false;
	}

	return true;
}
