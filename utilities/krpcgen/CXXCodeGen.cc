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
CXXCodeGen::CXXCodeGen(Service *service) : CodeGen(service) {}

/** Generate server code.
 * @param path		Path to output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateServer(const string &path) {
	if(!GenerateServerHeader(GetHeaderPath(path)) || !GenerateServerCode(path)) {
		return false;
	}
	return true;
}

/** Generate client code.
 * @param path		Path to output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateClient(const string &path) {
	if(!GenerateClientHeader(GetHeaderPath(path)) || !GenerateClientCode(path)) {
		return false;
	}
	return true;
}

/** Generate the server header.
 * @param fpath		Path to the header file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateServerHeader(const std::string &path) {
	ofstream stream;
	if(!BeginHeader(path, stream)) {
		return false;
	}

	/* Write out the ClientConnection class definition. */
	stream << "class ClientConnection : public ::kiwi::RPCClientConnection {" << endl;
	stream << "public:" << endl;
	BOOST_FOREACH(const Function *event, m_service->GetEvents()) {
		stream << "	void " << event->GetName() << '(';
		stream << GetFunctionParams(event) << ");" << endl;
	}
	stream << "protected:" << endl;
	stream << "	ClientConnection(handle_t handle);" << endl;
	BOOST_FOREACH(const Function *func, m_service->GetFunctions()) {
		stream << "	virtual status_t " << func->GetName() << '(';
		stream << GetFunctionParams(func) << ") = 0;" << endl;
	}
	stream << "private:" << endl;
	stream << "	void HandleMessage(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* Finish the header. */
	EndHeader(stream);
	return true;
}

/** Generate the server code.
 * @param path		Path to the output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateServerCode(const std::string &path) {
	ofstream stream;
	if(!BeginCode(path, stream)) {
		return false;
	}

	/* Generate the constructor. */
	stream << "ClientConnection::ClientConnection(handle_t handle) : ::kiwi::RPCClientConnection(";
	stream << '"' << m_service->GetName() << "\", " << m_service->GetVersion();
	stream << ", handle) {}" << endl;

	/* Generate the event calls. */
	BOOST_FOREACH(const Function *event, m_service->GetEvents()) {
		stream << "void ClientConnection::" << event->GetName() << '(';
		stream << GetFunctionParams(event) << ") {" << endl;
		stream << "	::kiwi::RPCMessageBuffer __buf;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, event->GetParameters()) {
			stream << "	__buf << " << param.name << ';' << endl;
		}
		stream << "	SendMessage(" << event->GetMessageID() << ", __buf);" << endl;
		stream << '}' << endl;
	}

	/* Generate the message handler. */
	stream << "void ClientConnection::HandleMessage(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf) {" << endl;
	stream << "	switch(__id) {" << endl;
	BOOST_FOREACH(const Function *func, m_service->GetFunctions()) {
		stream << "	case " << func->GetMessageID() << ": {" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->GetParameters()) {
			stream << "		" << GetCXXType(param.type) << ' ';
			stream << param.name << ';' << endl;
			if(!param.out) {
				stream << "		__buf >> " << param.name << ';' << endl;
			}
		}
		stream << "		__buf.reset();" << endl;
		stream << "		status_t __ret = " << GetHandlerCall(func) << ';' << endl;
		stream << "		__buf << __ret;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->GetParameters()) {
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
	EndCode(stream);
	return true;
}

/** Generate the client header.
 * @param fpath		Path to the header file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateClientHeader(const std::string &path) {
	ofstream stream;
	if(!BeginHeader(path, stream)) {
		return false;
	}

	/* Write out the ServerConnection class definition. */
	stream << "class ServerConnection : public ::kiwi::RPCServerConnection {" << endl;
	stream << "public:" << endl;
	stream << "	ServerConnection();" << endl;
	stream << "	ServerConnection(port_id_t id);" << endl;
	BOOST_FOREACH(const Function *func, m_service->GetFunctions()) {
		stream << "	status_t " << func->GetName() << '(';
		stream << GetFunctionParams(func) << ");" << endl;
	}
	BOOST_FOREACH(const Function *event, m_service->GetEvents()) {
		stream << "	::kiwi::Signal<" << GetEventParams(event);
		stream << "> " << event->GetName() << ';' << endl;
	}
	stream << "private:" << endl;
	stream << "	void HandleEvent(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* Finish the header. */
	EndHeader(stream);
	return true;
}

/** Generate the client code.
 * @param path		Path to the output file.
 * @return		Whether generated successfully. */
bool CXXCodeGen::GenerateClientCode(const std::string &path) {
	ofstream stream;
	if(!BeginCode(path, stream)) {
		return false;
	}

	/* Generate the constructors. */
	stream << "ServerConnection::ServerConnection() : ::kiwi::RPCServerConnection(";
	stream << '"' << m_service->GetName() << "\", " << m_service->GetVersion();
	stream << ") {}" << endl;
	stream << "ServerConnection::ServerConnection(port_id_t id) : ::kiwi::RPCServerConnection(";
	stream << '"' << m_service->GetName() << "\", " << m_service->GetVersion();
	stream << ", id) {}" << endl;

	/* Generate the function calls. */
	BOOST_FOREACH(const Function *func, m_service->GetFunctions()) {
		stream << "status_t ServerConnection::" << func->GetName() << '(';
		stream << GetFunctionParams(func) << ") {" << endl;
		stream << "	::kiwi::RPCMessageBuffer __buf;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->GetParameters()) {
			if(!param.out) {
				stream << "	__buf << " << param.name << ';' << endl;
			}
		}
		stream << "	SendMessage(" << func->GetMessageID() << ", __buf);" << endl;
		stream << "	status_t __ret;" << endl;
		stream << "	__buf >> __ret;" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->GetParameters()) {
			if(param.out) {
				stream << "	__buf >> " << param.name << ';' << endl;
			}
		}
		stream << "	return __ret;" << endl;
		stream << '}' << endl;
	}

	/* Generate the event handler. */
	stream << "void ServerConnection::HandleEvent(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf) {" << endl;
	stream << "	switch(__id) {" << endl;
	BOOST_FOREACH(const Function *event, m_service->GetEvents()) {
		stream << "	case " << event->GetMessageID() << ": {" << endl;
		BOOST_FOREACH(const Function::Parameter &param, event->GetParameters()) {
			stream << "		" << GetCXXType(param.type) << ' ' << param.name << ';' << endl;
			stream << "		__buf >> " << param.name << ';' << endl;
		}
		stream << "		" << GetHandlerCall(event) << ';' << endl;
		stream << "		break;" << endl;
		stream << "	}" << endl;
	}
	stream << "	default: {" << endl;
	stream << "		std::stringstream __msg;" << endl;
	stream << "		__msg << \"Received unknown event ID \" << __id;" << endl;
	stream << "		throw ::kiwi::RPCError(__msg.str());" << endl;
	stream << "	}" << endl;
	stream << "	}" << endl;
	stream << '}' << endl;

	/* Finish the code and close the stream. */
	EndCode(stream);
	return true;
}

/** Open the header file and write the common start.
 * @param path		Path to header file.
 * @param stream	Stream to use. Will refer to the header file when the
 *			function returns.
 * @return		Whether successful in opening the header. */
bool CXXCodeGen::BeginHeader(const string &path, ofstream &stream) {
	/* Open the file. */
	stream.open(path.c_str(), ofstream::trunc);
	if(stream.fail()) {
		cerr << "Failed to create header file `" << path << "'." << endl;
		return false;
	}

	/* Generate the include guard name. */
	string guard = "__KRPCGEN_CXX_HEADER_";
	BOOST_FOREACH(char ch, m_service->GetName()) {
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
	StartNamespace(stream);

	/* Now write out definitions for type aliases and structures. */
	BOOST_FOREACH(const Service::TypeMap::value_type &type, m_service->GetTypes()) {
		AliasType *atype = dynamic_cast<AliasType *>(type.second);
		if(atype) {
			stream << "typedef " << GetCXXType(atype->Resolve());
			stream << ' ' << atype->GetName() << ';' << endl;
			continue;
		}

		StructType *stype = dynamic_cast<StructType *>(type.second);
		if(stype) {
			/* Write the structure definition. */
			stream << "struct " << stype->GetName() << " {" << endl;
			BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->GetEntries()) {
				stream << '\t' << GetCXXType(ent.first);
				stream << ' ' << ent.second << ';' << endl;
			}
			stream << "};" << endl;

			/* Write the (un)serialisation function definitions. */
			stream << "::kiwi::RPCMessageBuffer &operator <<(::kiwi::RPCMessageBuffer &a, ";
			stream << "const " << stype->GetName() << " &b);" << endl;
			stream << "::kiwi::RPCMessageBuffer &operator >>(::kiwi::RPCMessageBuffer &a, ";
			stream << stype->GetName() << " &b);" << endl;
		}
	}

	return true;
}

/** Finish and close the header file.
 * @param stream	Stream for the header. */
void CXXCodeGen::EndHeader(ofstream &stream) {
	EndNamespace(stream);
	stream << "#endif" << endl;
	stream.close();
}

/** Open the output file and write the common start.
 * @param path		Path to output file.
 * @param stream	Stream to use. Will refer to the file when the function
 *			returns.
 * @return		Whether successful in opening the file. */
bool CXXCodeGen::BeginCode(const string &path, ofstream &stream) {
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
	string hpath = GetHeaderPath(path);
	size_t idx = hpath.find_last_of('/');
	if(idx != string::npos) {
		stream << string(hpath, idx + 1);
	} else {
		stream << hpath;
	}
	stream << "\"" << endl;
	StartNamespace(stream);

	/* Write the struct (un)serialisation functions. */
	BOOST_FOREACH(const Service::TypeMap::value_type &type, m_service->GetTypes()) {
		StructType *stype = dynamic_cast<StructType *>(type.second);
		if(!stype) {
			continue;
		}

		stream << "::kiwi::RPCMessageBuffer &operator <<(::kiwi::RPCMessageBuffer &a, ";
		stream << "const " << stype->GetName() << " &b) {" << endl;
		BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->GetEntries()) {
			stream << "	a << b." << ent.second << ';' << endl;
		}
		stream << "	return a;" << endl;
		stream << "}" << endl;

		stream << "::kiwi::RPCMessageBuffer &operator >>(::kiwi::RPCMessageBuffer &a, ";
		stream << stype->GetName() << " &b) {" << endl;
		BOOST_FOREACH(const StructType::EntryList::value_type &ent, stype->GetEntries()) {
			stream << "	a >> b." << ent.second << ';' << endl;
		}
		stream << "	return a;" << endl;
		stream << "}" << endl;
	}

	return true;
}

/** Finish and close the output file.
 * @param stream	Stream for the file. */
void CXXCodeGen::EndCode(ofstream &stream) {
	EndNamespace(stream);
	stream.close();
}

/** Write out the namespace start.
 * @param stream	Stream to write to. */
void CXXCodeGen::StartNamespace(ofstream &stream) {
	vector<string> tokens;
	m_service->TokeniseName(tokens);
	BOOST_FOREACH(string &str, tokens) {
		stream << "namespace " << str << " {" << endl;
	}
}

/** Write out the namespace end.
 * @param stream	Stream to write to. */
void CXXCodeGen::EndNamespace(ofstream &stream) {
	vector<string> tokens;
	m_service->TokeniseName(tokens);
	BOOST_FOREACH(string &str, tokens) {
		(void)str;
		stream << "}" << endl;
	}
}

/** Get the header file path.
 * @param fpath		Path to the main output file.
 * @return		Path to header file. */
string CXXCodeGen::GetHeaderPath(const std::string &fpath) {
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
string CXXCodeGen::GetCXXType(Type *type) {
	if(IntegerType *itype = dynamic_cast<IntegerType *>(type)) {
		ostringstream str;
		if(!itype->IsSigned()) { str << 'u'; }
		str << "int" << itype->GetWidth() << "_t";
		return str.str();
	} else if(dynamic_cast<BooleanType *>(type)) {
		return "bool";
	} else if(dynamic_cast<StringType *>(type)) {
		return "::std::string";
	} else if(dynamic_cast<BytesType *>(type)) {
		return "::kiwi::RPCByteString";
	} else {
		return type->GetName();
	}
}

/** Generate a string containing a function's parameter list.
 * @param func		Function to generate for.
 * @return		String parameter list. */
string CXXCodeGen::GetFunctionParams(const Function *func) {
	bool first = true;
	ostringstream str;

	BOOST_FOREACH(const Function::Parameter &p, func->GetParameters()) {
		if(!first) { str << ", "; }
		if(p.out) {
			str << GetCXXType(p.type) << " &";
		} else {
			if(dynamic_cast<StringType *>(p.type)) {
				str << "const ::std::string &";
			} else {
				str << GetCXXType(p.type) << ' ';
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
string CXXCodeGen::GetEventParams(const Function *event) {
	bool first = true;
	ostringstream str;

	BOOST_FOREACH(const Function::Parameter &p, event->GetParameters()) {
		if(!first) { str << ", "; }
		if(dynamic_cast<StringType *>(p.type)) {
			str << "const ::std::string &";
		} else {
			str << GetCXXType(p.type);
		}
		first = false;
	}

	return str.str();
}

/** Generate a call to an message handler.
 * @param func		Function to generate for.
 * @return		String containing call. */
string CXXCodeGen::GetHandlerCall(const Function *func) {
	bool first = true;
	ostringstream str;

	str << func->GetName() << '(';
	BOOST_FOREACH(const Function::Parameter &p, func->GetParameters()) {
		if(!first) { str << ", "; }
		str << p.name;
		first = false;
	}
	str << ')';
	return str.str();
}
