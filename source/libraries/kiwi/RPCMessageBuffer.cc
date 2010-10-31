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

#include <cstring>
#include <endian.h>
#include <sstream>

using namespace kiwi;

/** Construct an RPC error object. */
RPCError::RPCError(const std::string &msg) : m_msg(msg) {}

/** Destroy an RPC error object. */
RPCError::~RPCError() throw() {}

/** Get the description of an RPC error.
 * @return		Description of the error. */
const char *RPCError::GetDescription() const throw() {
	return m_msg.c_str();
}

/** Construct an empty message buffer. */
RPCMessageBuffer::RPCMessageBuffer() :
	m_buffer(0), m_size(0), m_offset(0)
{
}

/** Construct a message buffer.
 * @param buf		Buffer to use. Must be allocated via operator new[].
 *			The object will take ownership of this buffer, and may
 *			be deleted at any time.
 * @param size		Size of the buffer. */
RPCMessageBuffer::RPCMessageBuffer(char *buf, size_t size) :
	m_buffer(buf), m_size(size), m_offset(0)
{
}

/** Destroy a message buffer. */
RPCMessageBuffer::~RPCMessageBuffer() {
	Reset();
}

/** Reset a message buffer.
 * @param buf		New buffer to use. Will be taken over by the object.
 * @param size		Size of the buffer. */
void RPCMessageBuffer::Reset(char *buf, size_t size) {
	if(m_buffer) {
		delete[] m_buffer;
	}
	m_buffer = buf;
	m_size = size;
	m_offset = 0;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(bool val) {
	/* Ensure that bool is kept a standard size across machines. */
	uint8_t rval = static_cast<uint8_t>(val);
	PushEntry(kBoolType, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(const std::string &str) {
	PushEntry(kStringType, str.c_str(), str.length());
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(RPCByteString &bytes) {
	PushEntry(kBytesType, bytes.data, bytes.size);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int8_t val) {
	PushEntry(kInt8Type, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int16_t val) {
	int16_t rval = cpu_to_le16(val);
	PushEntry(kInt16Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int32_t val) {
	int32_t rval = cpu_to_le32(val);
	PushEntry(kInt32Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int64_t val) {
	int64_t rval = cpu_to_le64(val);
	PushEntry(kInt64Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint8_t val) {
	PushEntry(kUint8Type, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint16_t val) {
	uint16_t rval = cpu_to_le16(val);
	PushEntry(kUint16Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint32_t val) {
	uint32_t rval = cpu_to_le32(val);
	PushEntry(kUint32Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint64_t val) {
	uint64_t rval = cpu_to_le64(val);
	PushEntry(kUint64Type, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(bool &val) {
	/* Boolean is transmitted as uint8_t. */
	uint8_t rval;
	PopEntry(kBoolType, rval);
	val = static_cast<bool>(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(std::string &str) {
	const char *buf;
	size_t size;
	PopEntry(kStringType, buf, size);
	str = std::string(buf, size);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(RPCByteString &bytes) {
	const char *buf;
	size_t size;
	PopEntry(kBytesType, buf, size);
	bytes = RPCByteString(buf, size);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int8_t &val) {
	PopEntry(kInt8Type, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int16_t &val) {
	int16_t rval;
	PopEntry(kInt16Type, rval);
	val = le16_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int32_t &val) {
	int32_t rval;
	PopEntry(kInt32Type, rval);
	val = le32_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int64_t &val) {
	int64_t rval;
	PopEntry(kInt64Type, rval);
	val = le64_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint8_t &val) {
	PopEntry(kUint8Type, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint16_t &val) {
	uint16_t rval;
	PopEntry(kUint16Type, rval);
	val = le16_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint32_t &val) {
	uint32_t rval;
	PopEntry(kUint32Type, rval);
	val = le32_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint64_t &val) {
	uint64_t rval;
	PopEntry(kUint64Type, rval);
	val = le64_to_cpu(rval);
	return *this;
}

/** Push an entry into the buffer.
 * @param type		ID of the type of the entry.
 * @param entry		Entry to push. */
template <typename T>
void RPCMessageBuffer::PushEntry(TypeID type, T entry) {
	PushEntry(type, reinterpret_cast<const char *>(&entry), sizeof(entry));
}

/** Pop an entry from the buffer.
 * @param type		ID of the type expected.
 * @param entry		Where to store entry popped. */
template <typename T>
void RPCMessageBuffer::PopEntry(TypeID type, T &entry) {
	const char *buf;
	size_t size;
	PopEntry(type, buf, size);
	if(size != sizeof(T)) {
		throw RPCError("Message entry size not as expected");
	}
	entry = *reinterpret_cast<const T *>(buf);
}

/** Push an entry into the buffer.
 * @param type		ID of the type of the entry.
 * @param data		Data for entry to push.
 * @param size		Size of the entry. */
void RPCMessageBuffer::PushEntry(TypeID type, const char *data, size_t size) {
	/* The entry contains a 1 byte type ID, a 4 byte entry size and the
	 * data itself. */
	size_t total = size + 5;

	/* Check if there is space. */
	if((m_offset + total) > m_size) {
		char *nbuf = new char[m_offset + total];
		memcpy(nbuf, m_buffer, m_size);
		delete[] m_buffer;
		m_buffer = nbuf;
		m_size = m_offset + total;
	}

	*reinterpret_cast<uint8_t *>(&m_buffer[m_offset]) = static_cast<uint8_t>(type);
	*reinterpret_cast<uint32_t *>(&m_buffer[m_offset + 1]) = static_cast<uint32_t>(size);
	memcpy(&m_buffer[m_offset + 5], data, size);
	m_offset += total;
}

/** Pop an entry from the buffer.
 * @param type		ID of the type expected.
 * @param data		Where to store pointer to entry data.
 * @param size		Where to store size of the entry. */
void RPCMessageBuffer::PopEntry(TypeID type, const char *&data, size_t &size) {
	if((m_offset + 5) > m_size) {
		throw RPCError("Message buffer smaller than expected");
	}

	TypeID rtype = static_cast<TypeID>(*reinterpret_cast<uint8_t *>(&m_buffer[m_offset]));
	if(rtype != type) {
		std::ostringstream msg;
		msg << "Message entry type (" << static_cast<int>(rtype);
		msg << ") not as expected (" << static_cast<int>(type) << ')';
		throw RPCError(msg.str());
	}

	size = static_cast<size_t>(*reinterpret_cast<uint32_t *>(&m_buffer[m_offset + 1]));
	data = &m_buffer[m_offset + 5];
	m_offset += size + 5;
}
