/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
