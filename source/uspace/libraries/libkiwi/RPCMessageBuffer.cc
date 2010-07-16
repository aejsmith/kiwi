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
#include <stdexcept>

using namespace kiwi;

/** Construct an empty message buffer. */
RPCMessageBuffer::RPCMessageBuffer() :
	m_buffer(NULL), m_size(0), m_offset(0)
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
	delete[] m_buffer;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(bool val) {
	/* Ensure that bool is kept a standard size across machines. */
	uint8_t rval = static_cast<uint8_t>(val);
	pushEntry(TYPE_BOOL, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(const std::string &str) {
	pushEntry(TYPE_STRING, str.c_str(), str.length());
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(RPCByteString &bytes) {
	pushEntry(TYPE_BYTES, bytes.first, bytes.second);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int8_t val) {
	pushEntry(TYPE_INT8, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int16_t val) {
	int16_t rval = cpu_to_le16(val);
	pushEntry(TYPE_INT16, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int32_t val) {
	int32_t rval = cpu_to_le32(val);
	pushEntry(TYPE_INT32, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(int64_t val) {
	int64_t rval = cpu_to_le64(val);
	pushEntry(TYPE_INT64, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint8_t val) {
	pushEntry(TYPE_UINT8, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint16_t val) {
	uint16_t rval = cpu_to_le16(val);
	pushEntry(TYPE_UINT16, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint32_t val) {
	uint32_t rval = cpu_to_le32(val);
	pushEntry(TYPE_UINT32, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator <<(uint64_t val) {
	uint64_t rval = cpu_to_le64(val);
	pushEntry(TYPE_UINT64, rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(bool &val) {
	/* Boolean is transmitted as uint8_t. */
	uint8_t rval;
	popEntry(TYPE_BOOL, rval);
	val = static_cast<bool>(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(std::string &str) {
	const char *buf;
	size_t size;
	popEntry(TYPE_STRING, buf, size);
	str = std::string(buf, size);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(RPCByteString &bytes) {
	const char *buf;
	size_t size;
	popEntry(TYPE_BYTES, buf, size);
	bytes = RPCByteString(buf, size);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int8_t &val) {
	popEntry(TYPE_INT8, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int16_t &val) {
	int16_t rval;
	popEntry(TYPE_INT16, rval);
	val = le16_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int32_t &val) {
	int32_t rval;
	popEntry(TYPE_INT32, rval);
	val = le32_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(int64_t &val) {
	int64_t rval;
	popEntry(TYPE_INT64, rval);
	val = le64_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint8_t &val) {
	popEntry(TYPE_UINT8, val);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint16_t &val) {
	uint16_t rval;
	popEntry(TYPE_UINT64, rval);
	val = le16_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint32_t &val) {
	uint32_t rval;
	popEntry(TYPE_UINT32, rval);
	val = le32_to_cpu(rval);
	return *this;
}

RPCMessageBuffer &RPCMessageBuffer::operator >>(uint64_t &val) {
	uint64_t rval;
	popEntry(TYPE_UINT64, rval);
	val = le64_to_cpu(rval);
	return *this;
}

/** Push an entry into the buffer.
 * @param type		ID of the type of the entry.
 * @param entry		Entry to push. */
template <typename T>
void RPCMessageBuffer::pushEntry(TypeID type, T entry) {
	pushEntry(type, reinterpret_cast<const char *>(&entry), sizeof(entry));
}

/** Pop an entry from the buffer.
 * @param type		ID of the type expected.
 * @param entry		Where to store entry popped. */
template <typename T>
void RPCMessageBuffer::popEntry(TypeID type, T &entry) {
	const char *buf;
	size_t size;
	popEntry(type, buf, size);
	if(size != sizeof(T)) {
		throw std::runtime_error("Messag entry size not as expected");
	}
	entry = *reinterpret_cast<const T *>(buf);
}

/** Push an entry into the buffer.
 * @param type		ID of the type of the entry.
 * @param data		Data for entry to push.
 * @param size		Size of the entry. */
void RPCMessageBuffer::pushEntry(TypeID type, const char *data, size_t size) {
	/* The entry contains a 1 byte type ID, a 4 byte entry size and the
	 * data itself. */
	size_t total = size + 5;

	/* Check if there is space. */
	if((m_offset + total) > m_size) {
		char *nbuf = new char[m_offset + total];
		memcpy(nbuf, m_buffer, m_size);
		delete m_buffer;
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
void RPCMessageBuffer::popEntry(TypeID type, const char *&data, size_t &size) {
	if((m_offset + 5) > m_size) {
		throw std::runtime_error("Message buffer smaller than expected");
	}

	TypeID rtype = static_cast<TypeID>(*reinterpret_cast<uint8_t *>(&m_buffer[m_offset]));
	if(rtype != type) {
		throw std::runtime_error("Message entry type not as expected");
	}

	size = static_cast<size_t>(*reinterpret_cast<uint32_t *>(&m_buffer[m_offset + 1]));
	data = &m_buffer[m_offset + 5];
	m_offset += size + 5;
}
