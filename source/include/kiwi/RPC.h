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
 * @brief		RPC base classes/types.
 *
 * @todo		Once String is implemented, replace std::string usage.
 */

#ifndef __KIWI_RPC_H
#define __KIWI_RPC_H

#include <kiwi/Error.h>
#include <kiwi/IPCConnection.h>

#include <string>

namespace kiwi {

/** Exception thrown for RPC protocol errors. */
class KIWI_PUBLIC RPCError : public BaseError {
public:
	explicit RPCError(const std::string &msg);
	~RPCError() throw();
	const char *GetDescription() const throw();
private:
	std::string m_msg;		/**< Error message. */
};

/** Type implementing the RPC 'bytes' type. */
struct KIWI_PUBLIC RPCByteString {
	RPCByteString(const char *d, size_t s) : data(d), size(s) {}

	const char *data;		/**< Data buffer. */
	size_t size;			/**< Size of buffer. */
};

/** Class used to store an RPC message buffer. */
class KIWI_PUBLIC RPCMessageBuffer : Noncopyable {
	/** Type IDs. */
	enum TypeID {
		kBoolType = 0,
		kStringType = 1,
		kBytesType = 2,
		kInt8Type = 3,
		kInt16Type = 4,
		kInt32Type = 5,
		kInt64Type = 6,
		kUint8Type = 7,
		kUint16Type = 8,
		kUint32Type = 9,
		kUint64Type = 10,
	};
public:
	RPCMessageBuffer();
	RPCMessageBuffer(char *buf, size_t size);
	RPCMessageBuffer(RPCMessageBuffer &&other);
	~RPCMessageBuffer();

	void Reset(char *buf = 0, size_t size = 0);

	RPCMessageBuffer &operator <<(bool val);
	RPCMessageBuffer &operator <<(const std::string &str);
	RPCMessageBuffer &operator <<(RPCByteString &bytes);
	RPCMessageBuffer &operator <<(int8_t val);
	RPCMessageBuffer &operator <<(int16_t val);
	RPCMessageBuffer &operator <<(int32_t val);
	RPCMessageBuffer &operator <<(int64_t val);
	RPCMessageBuffer &operator <<(uint8_t val);
	RPCMessageBuffer &operator <<(uint16_t val);
	RPCMessageBuffer &operator <<(uint32_t val);
	RPCMessageBuffer &operator <<(uint64_t val);

	RPCMessageBuffer &operator >>(bool &val);
	RPCMessageBuffer &operator >>(std::string &str);
	RPCMessageBuffer &operator >>(RPCByteString &bytes);
	RPCMessageBuffer &operator >>(int8_t &val);
	RPCMessageBuffer &operator >>(int16_t &val);
	RPCMessageBuffer &operator >>(int32_t &val);
	RPCMessageBuffer &operator >>(int64_t &val);
	RPCMessageBuffer &operator >>(uint8_t &val);
	RPCMessageBuffer &operator >>(uint16_t &val);
	RPCMessageBuffer &operator >>(uint32_t &val);
	RPCMessageBuffer &operator >>(uint64_t &val);

	/** Get the message data buffer.
	 * @return		Message data buffer. */
	const char *GetBuffer() { return m_buffer; }

	/** Get the buffer size.
	 * @return		Message data buffer size. */
	size_t GetSize() { return m_size; }
private:
	template <typename T>
	KIWI_PRIVATE void PushEntry(TypeID type, T entry);

	template <typename T>
	KIWI_PRIVATE void PopEntry(TypeID type, T &entry);

	KIWI_PRIVATE void PushEntry(TypeID type, const char *data, size_t size);
	KIWI_PRIVATE void PopEntry(TypeID type, const char *&data, size_t &size);

	char *m_buffer;			/**< Buffer containing message data. */
	size_t m_size;			/**< Current buffer size. */
	size_t m_offset;		/**< Current buffer offset. */
};

/** Base class for a connection to a server. */
class KIWI_PUBLIC RPCServerConnection : public Object {
public:
	void Connect(port_id_t id = -1);
protected:
	RPCServerConnection(const char *name, uint32_t version, handle_t handle = -1);

	void SendMessage(uint32_t id, RPCMessageBuffer &buf);
	void ReceiveMessage(uint32_t &id, RPCMessageBuffer &buf);
	virtual void HandleEvent(uint32_t id, RPCMessageBuffer &buf);
private:
	void HandleMessage();
	void CheckVersion();

	IPCConnection m_conn;		/**< Real connection to the server. */
	const char *m_name;		/**< Name of the service. */
	uint32_t m_version;		/**< Service version that the connection is for. */
};

/** Base class for a connection to a client. */
class KIWI_PUBLIC RPCClientConnection : public Object {
protected:
	RPCClientConnection(const char *name, uint32_t version, handle_t handle);

	void SendMessage(uint32_t id, RPCMessageBuffer &buf);
	virtual void HandleMessage(uint32_t id, RPCMessageBuffer &buf) = 0;
	virtual void HandleHangup();
private:
	void _HandleMessage();

	IPCConnection m_conn;		/**< Real connection to the client. */
	const char *m_name;		/**< Name of the service. */
	uint32_t m_version;		/**< Service version that the connection is for. */
};

}

#endif /* __KIWI_RPC_H */
