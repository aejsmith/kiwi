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
 * @brief		C++ RPC code generator.
 */

#include <boost/foreach.hpp>
#include <ctype.h>
#include <iostream>
#include <sstream>

#include "CXXCodeGen.h"

using namespace std;

/** Constructor for a C++ code generator. */
CXXCodeGen::CXXCodeGen(Service *service) :
	CodeGen(service)
{
}

/** Generate server code.
 * @param path		Path to output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateServer(const string &path) {
	if(!generateServerHeader(getHeaderPath(path)) || !generateServerCode(path)) {
		return false;
	}
	return true;
}

/** Generate client code.
 * @param path		Path to output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateClient(const string &path) {
	if(!generateClientHeader(getHeaderPath(path)) || !generateClientCode(path)) {
		return false;
	}
	return true;
}

/** Generate the server header.
 * @param fpath		Path to the header file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateServerHeader(const std::string &path) {
	ofstream stream;
	if(!beginHeader(path, stream)) {
		return false;
	}

	/* Write out the ClientConnection class definition. */
	stream << "class ClientConnection : public ::kiwi::RPCClientConnection {" << endl;
	stream << "public:" << endl;
	BOOST_FOREACH(const Function *event, m_service->getEvents()) {
		stream << "	void " << event->getName() << '(';
		stream << getFunctionParams(event) << ");" << endl;
	}
	stream << "protected:" << endl;
	stream << "	ClientConnection(handle_t handle);" << endl;
	BOOST_FOREACH(const Function *func, m_service->getFunctions()) {
		stream << "	virtual ::kiwi::RPCResult " << func->getName() << '(';
		stream << getFunctionParams(func) << ") = 0;" << endl;
	}
	stream << "private:" << endl;
	stream << "	void handleMessage(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* Finish the header. */
	endHeader(stream);
	return true;
}

/** Generate the server code.
 * @param path		Path to the output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateServerCode(const std::string &path) {
	ofstream stream;
	if(!beginCode(path, stream)) {
		return false;
	}

	/* Generate the constructor. */
	stream << "ClientConnection::ClientConnection(handle_t handle) : ::kiwi::RPCClientConnection(";
	stream << '"' << m_service->getName() << "\", " << m_service->getVersion();
	stream << ", handle) {}" << endl;

	/* Generate the event calls. */
	BOOST_FOREACH(const Function *event, m_service->getEvents()) {
		stream << "void ClientConnection::" << event->getName() << '(';
		stream << getFunctionParams(event) << ") {" << endl;
		stream << "	::kiwi::RPCMessageBuffer __buf;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, event->getParameters()) {
			stream << "	__buf << " << param.name << ';' << endl;
		}
		stream << "	sendMessage(" << event->getMessageID() << ", __buf);" << endl;
		stream << '}' << endl;
	}

	/* Generate the message handler. */
	stream << "void ClientConnection::handleMessage(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf) {" << endl;
	stream << "	switch(__id) {" << endl;
	BOOST_FOREACH(const Function *func, m_service->getFunctions()) {
		stream << "	case " << func->getMessageID() << ": {" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->getParameters()) {
			stream << "		" << getCXXType(param.type) << ' ';
			stream << param.name << ';' << endl;
			if(!param.out) {
				stream << "		__buf >> " << param.name << ';' << endl;
			}
		}
		stream << "		__buf.reset();" << endl;
		stream << "		::kiwi::RPCResult __ret = " << getHandlerCall(func) << ';' << endl;
		stream << "		__buf << __ret;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->getParameters()) {
			if(param.out) {
				stream << "		__buf << " << param.name << ';' << endl;
			}
		}
		stream << "		break;" << endl;
		stream << "	}" << endl;
	}
	stream << "	}" << endl;
	stream << '}' << endl;

	/* Finish the code and close the stream. */
	endCode(stream);
	return true;
}

/** Generate the client header.
 * @param fpath		Path to the header file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateClientHeader(const std::string &path) {
	ofstream stream;
	if(!beginHeader(path, stream)) {
		return false;
	}

	/* Write out the ServerConnection class definition. */
	stream << "class ServerConnection : public ::kiwi::RPCServerConnection {" << endl;
	stream << "public:" << endl;
	stream << "	ServerConnection();" << endl;
	BOOST_FOREACH(const Function *func, m_service->getFunctions()) {
		stream << "	::kiwi::RPCResult " << func->getName() << '(';
		stream << getFunctionParams(func) << ");" << endl;
	}
	BOOST_FOREACH(const Function *event, m_service->getEvents()) {
		stream << "	::kiwi::Signal<" << getEventParams(event);
		stream << "> " << event->getName() << ';' << endl;
	}
	stream << "private:" << endl;
	stream << "	void handleEvent(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* Finish the header. */
	endHeader(stream);
	return true;
}

/** Generate the client code.
 * @param path		Path to the output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::generateClientCode(const std::string &path) {
	ofstream stream;
	if(!beginCode(path, stream)) {
		return false;
	}

	/* Generate the constructor. */
	stream << "ServerConnection::ServerConnection() : ::kiwi::RPCServerConnection(";
	stream << '"' << m_service->getName() << "\", " << m_service->getVersion();
	stream << ") {}" << endl;

	/* Generate the function calls. */
	BOOST_FOREACH(const Function *func, m_service->getFunctions()) {
		stream << "::kiwi::RPCResult ServerConnection::" << func->getName() << '(';
		stream << getFunctionParams(func) << ") {" << endl;
		stream << "	::kiwi::RPCMessageBuffer __buf;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->getParameters()) {
			if(!param.out) {
				stream << "	__buf << " << param.name << ';' << endl;
			}
		}
		stream << "	sendMessage(" << func->getMessageID() << ", __buf);" << endl;
		stream << "	::kiwi::RPCResult __ret;" << endl;
		stream << "	__buf >> __ret;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->getParameters()) {
			if(param.out) {
				stream << "	__buf >> " << param.name << ';' << endl;
			}
		}
		stream << "	return __ret;" << endl;
		stream << '}' << endl;
	}

	/* Generate the event handler. */
	stream << "void ServerConnection::handleEvent(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf) {" << endl;
	stream << "	switch(__id) {" << endl;
	BOOST_FOREACH(const Function *event, m_service->getEvents()) {
		stream << "	case " << event->getMessageID() << ": {" << endl;
		BOOST_FOREACH(const Function::Parameter &param, event->getParameters()) {
			stream << "		" << getCXXType(param.type) << ' ' << param.name << ';' << endl;
			stream << "		__buf >> " << param.name << ';' << endl;
		}
		stream << "		" << getHandlerCall(event) << ';' << endl;
		stream << "		break;" << endl;
		stream << "	}" << endl;
	}
	stream << "	default: {" << endl;
	stream << "		std::stringstream __msg;" << endl;
	stream << "		__msg << \"Received unknown event ID \" << __id;" << endl;
	stream << "		throw std::runtime_error(__msg.str());" << endl;
	stream << "	}" << endl;
	stream << "	}" << endl;
	stream << '}' << endl;

	/* Finish the code and close the stream. */
	endCode(stream);
	return true;
}

/** Open the header file and write the common start.
 * @param path		Path to header file.
 * @param stream	Stream to use. Will refer to the header file when the
 *			function returns.
 * @return		Whether successful in opening the header. */
bool CXXCodeGen::beginHeader(const string &path, ofstream &stream) {
	/* Open the file. */
	stream.open(path.c_str(), ofstream::trunc);
	if(stream.fail()) {
		cerr << "Failed to create header file `" << path << "'." << endl;
		return false;
	}

	/* Generate the include guard name. */
	string guard = "__KRPCGEN_CXX_HEADER_";
	BOOST_FOREACH(char ch, m_service->getName()) {
		if(ch == '.') {
			guard += '_';
		} else if(isalnum(ch)) {
			guard += ch;
		}
	}

	/* Write the standard preamble. */
	stream << "/* This file is automatically generated. DO NOT EDIT! */" << endl;
	stream << "#ifndef " << guard << endl;
	stream << "#define " << guard << endl;
	stream << "#include <kiwi/RPC.h>" << endl;
	startNamespace(stream);

	/* Now write out definitions for type aliases and structures. */
	BOOST_FOREACH(const Service::TypeMap::value_type &type, m_service->getTypes()) {
		AliasType *atype = dynamic_cast<AliasType *>(type.second);
		if(atype) {
			stream << "typedef " << getCXXType(atype->resolve());
			stream << ' ' << atype->getName() << ';' << endl;
			continue;
		}

		StructType *stype = dynamic_cast<StructType *>(type.second);
		if(stype) {
			/* Write the structure definition. */
			stream << "struct " << stype->getName() << " {" << endl;
			BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->getEntries()) {
				stream << '\t' << getCXXType(ent.first);
				stream << ' ' << ent.second << ';' << endl;
			}
			stream << "};" << endl;

			/* Write the (un)serialisation function definitions. */
			stream << "::kiwi::RPCMessageBuffer &operator <<(::kiwi::RPCMessageBuffer &a, ";
			stream << "const " << stype->getName() << " &b);" << endl;
			stream << "::kiwi::RPCMessageBuffer &operator >>(::kiwi::RPCMessageBuffer &a, ";
			stream << stype->getName() << " &b);" << endl;
		}
	}

	return true;
}

/** Finish and close the header file.
 * @param stream	Stream for the header. */
void CXXCodeGen::endHeader(ofstream &stream) {
	endNamespace(stream);
	stream << "#endif" << endl;
	stream.close();
}

/** Open the output file and write the common start.
 * @param path		Path to output file.
 * @param stream	Stream to use. Will refer to the file when the function
 *			returns.
 * @return		Whether successful in opening the file. */
bool CXXCodeGen::beginCode(const string &path, ofstream &stream) {
	/* Open the file. */
	stream.open(path.c_str(), ofstream::trunc);
	if(stream.fail()) {
		cerr << "Failed to create output file `" << path << "'." << endl;
		return false;
	}

	/* Write the standard file beginning. */
	stream << "/* This file is automatically generated. DO NOT EDIT! */" << endl;
	stream << "#include <stdexcept>" << endl;
	stream << "#include <sstream>" << endl;
	stream << "#include \"";
	string hpath = getHeaderPath(path);
	size_t idx = hpath.find_last_of('/');
	if(idx != string::npos) {
		stream << string(hpath, idx + 1);
	} else {
		stream << hpath;
	}
	stream << "\"" << endl;
	startNamespace(stream);

	/* Write the struct (un)serialisation functions. */
	BOOST_FOREACH(const Service::TypeMap::value_type &type, m_service->getTypes()) {
		StructType *stype = dynamic_cast<StructType *>(type.second);
		if(!stype) {
			continue;
		}

		stream << "::kiwi::RPCMessageBuffer &operator <<(::kiwi::RPCMessageBuffer &a, ";
		stream << "const " << stype->getName() << " &b) {" << endl;
		BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->getEntries()) {
			stream << "	a << b." << ent.second << ';' << endl;
		}
		stream << "	return a;" << endl;
		stream << "}" << endl;

		stream << "::kiwi::RPCMessageBuffer &operator >>(::kiwi::RPCMessageBuffer &a, ";
		stream << stype->getName() << " &b) {" << endl;
		BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->getEntries()) {
			stream << "	a >> b." << ent.second << ';' << endl;
		}
		stream << "	return a;" << endl;
		stream << "}" << endl;
	}

	return true;
}

/** Finish and close the output file.
 * @param stream	Stream for the file. */
void CXXCodeGen::endCode(ofstream &stream) {
	endNamespace(stream);
	stream.close();
}

/** Write out the namespace start.
 * @param stream	Stream to write to. */
void CXXCodeGen::startNamespace(ofstream &stream) {
	vector<string> tokens;
	m_service->tokeniseName(tokens);
	BOOST_FOREACH(string &str, tokens) {
		stream << "namespace " << str << " {" << endl;
	}
}

/** Write out the namespace end.
 * @param stream	Stream to write to. */
void CXXCodeGen::endNamespace(ofstream &stream) {
	vector<string> tokens;
	m_service->tokeniseName(tokens);
	BOOST_FOREACH(string &str, tokens) {
		(void)str;
		stream << "}" << endl;
	}
}

/** Get the header file path.
 * @param fpath		Path to the main output file.
 * @return		Path to header file. */
string CXXCodeGen::getHeaderPath(const std::string &fpath) {
	size_t idx = fpath.find_last_of('.');
	if(idx != string::npos) {
		return string(fpath, 0, idx) + ".h";
	} else {
		return fpath + ".h";
	}
}

/** Generate the C++ name of a type.
 * @param type		Type to get name of.
 * @return		Name of type. */
string CXXCodeGen::getCXXType(Type *type) {
	IntegerType *itype = dynamic_cast<IntegerType *>(type);
	if(itype) {
		ostringstream str;
		if(!itype->isSigned()) { str << 'u'; }
		str << "int" << itype->getWidth() << "_t";
		return str.str();
	} else if(dynamic_cast<BooleanType *>(type)) {
		return "bool";
	} else if(dynamic_cast<StringType *>(type)) {
		return "::std::string";
	} else if(dynamic_cast<BytesType *>(type)) {
		return "::kiwi::RPCByteString";
	} else {
		return type->getName();
	}
}

/** Generate a string containing a function's parameter list.
 * @param func		Function to generate for.
 * @return		String parameter list. */
string CXXCodeGen::getFunctionParams(const Function *func) {
	bool first = true;
	ostringstream str;

	BOOST_FOREACH(const Function::Parameter &p, func->getParameters()) {
		if(!first) { str << ", "; }
		if(p.out) {
			str << getCXXType(p.type) << " &";
		} else {
			if(dynamic_cast<StringType *>(p.type)) {
				str << "const ::std::string &";
			} else {
				str << getCXXType(p.type) << ' ';
			}
		}
		str << p.name;
		first = false;
	}

	return str.str();
}

/** Generate a string containing an event's parameter list.
 * @param event		Event to generate for.
 * @return		String parameter list. */
string CXXCodeGen::getEventParams(const Function *event) {
	bool first = true;
	ostringstream str;

	BOOST_FOREACH(const Function::Parameter &p, event->getParameters()) {
		if(!first) { str << ", "; }
		if(dynamic_cast<StringType *>(p.type)) {
			str << "const ::std::string &";
		} else {
			str << getCXXType(p.type);
		}
		first = false;
	}

	return str.str();
}

/** Generate a call to an message handler.
 * @param func		Function to generate for.
 * @return		String containing call. */
string CXXCodeGen::getHandlerCall(const Function *func) {
	bool first = true;
	ostringstream str;

	str << func->getName() << '(';
	BOOST_FOREACH(const Function::Parameter &p, func->getParameters()) {
		if(!first) { str << ", "; }
		str << p.name;
		first = false;
	}
	str << ')';
	return str.str();
}
