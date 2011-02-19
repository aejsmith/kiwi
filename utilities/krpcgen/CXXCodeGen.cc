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
 * @brief		C++ RPC code generator.
 */

#include <boost/foreach.hpp>
#include <ctype.h>
#include <iostream>
#include <sstream>

#include "CXXCodeGen.h"

using namespace std;

/** Generate code.
 * @param service	Service to generate for.
 * @param path		Path to output file.
 * @param server	Whether to generate server code.
 * @param client	Whether to generate client code.
 * @return		Whether generated successfully. */
bool CXXCodeGen::Generate(Service *service, const string &path, bool server, bool client) {
	ofstream stream;

	/* Generate the header file. */
	if(!BeginHeader(service, GetHeaderPath(path), stream)) {
		return false;
	}
	if(server) {
		GenerateServerHeader(service, stream);
	}
	if(client) {
		GenerateClientHeader(service, stream);
	}
	EndHeader(stream);

	/* Generate the main source file. */
	if(!BeginCode(service, path, stream)) {
		return false;
	}
	if(server) {
		GenerateServerCode(service, stream);
	}
	if(client) {
		GenerateClientCode(service, stream);
	}
	EndCode(stream);
	return true;
}

/** Generate the server header.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::GenerateServerHeader(Service *service, std::ofstream &stream) {
	/* Begin the namespace for this service. */
	BeginNamespace(service, stream);

	/* Generate code for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		GenerateServerHeader(child, stream);
	}

	/* Write out the ClientConnection class definition. */
	stream << "class ClientConnection : public ::kiwi::RPCClientConnection {" << endl;
	stream << "public:" << endl;
	BOOST_FOREACH(const Function *event, service->GetEvents()) {
		stream << "	void " << event->GetName() << '(';
		stream << GetFunctionParams(event) << ");" << endl;
	}
	stream << "protected:" << endl;
	stream << "	ClientConnection(handle_t handle);" << endl;
	BOOST_FOREACH(const Function *func, service->GetFunctions()) {
		stream << "	virtual status_t " << func->GetName() << '(';
		stream << GetFunctionParams(func) << ") = 0;" << endl;
	}
	stream << "private:" << endl;
	stream << "	void HandleMessage(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* End the namespace. */
	EndNamespace(service, stream);
}

/** Generate the server code.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::GenerateServerCode(Service *service, std::ofstream &stream) {
	/* Begin the namespace for this service. */
	BeginNamespace(service, stream);

	/* Generate code for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		GenerateServerCode(child, stream);
	}

	/* Generate the constructor. */
	stream << "ClientConnection::ClientConnection(handle_t handle) : ::kiwi::RPCClientConnection(";
	stream << '"' << service->GetFullName() << "\", " << service->GetVersion();
	stream << ", handle) {}" << endl;

	/* Generate the event calls. */
	BOOST_FOREACH(const Function *event, service->GetEvents()) {
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
	BOOST_FOREACH(const Function *func, service->GetFunctions()) {
		stream << "	case " << func->GetMessageID() << ": {" << endl;
		BOOST_FOREACH(const Function::Parameter &param, func->GetParameters()) {
			stream << "		" << GetCXXType(param.type) << ' ';
			stream << param.name << ';' << endl;
			if(!param.out) {
				stream << "		__buf >> " << param.name << ';' << endl;
			}
		}
		stream << "		__buf.Reset();" << endl;
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

	/* End the namespace. */
	EndNamespace(service, stream);
}

/** Generate the client header.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::GenerateClientHeader(Service *service, std::ofstream &stream) {
	/* Begin the namespace for this service. */
	BeginNamespace(service, stream);

	/* Generate code for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		GenerateClientHeader(child, stream);
	}

	/* Write out the ServerConnection class definition. */
	stream << "class ServerConnection : public ::kiwi::RPCServerConnection {" << endl;
	stream << "public:" << endl;
	stream << "	ServerConnection(handle_t handle = -1);" << endl;
	BOOST_FOREACH(const Function *func, service->GetFunctions()) {
		stream << "	status_t " << func->GetName() << '(';
		stream << GetFunctionParams(func) << ");" << endl;
	}
	BOOST_FOREACH(const Function *event, service->GetEvents()) {
		stream << "	::kiwi::Signal<" << GetEventParams(event);
		stream << "> " << event->GetName() << ';' << endl;
	}
	stream << "private:" << endl;
	stream << "	void HandleEvent(uint32_t __id, ::kiwi::RPCMessageBuffer &__buf);" << endl;
	stream << "};" << endl;

	/* End the namespace. */
	EndNamespace(service, stream);
}

/** Generate the client code.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::GenerateClientCode(Service *service, std::ofstream &stream) {
	/* Begin the namespace for this service. */
	BeginNamespace(service, stream);

	/* Generate code for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		GenerateClientCode(child, stream);
	}

	/* Generate the constructor. */
	stream << "ServerConnection::ServerConnection(handle_t handle) : ::kiwi::RPCServerConnection(";
	stream << '"' << service->GetFullName() << "\", " << service->GetVersion();
	stream << ", handle) {}" << endl;

	/* Generate the function calls. */
	BOOST_FOREACH(const Function *func, service->GetFunctions()) {
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
	BOOST_FOREACH(const Function *event, service->GetEvents()) {
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
	stream << "		::kiwi::RPCServerConnection::HandleEvent(__id, __buf);" << endl;
	stream << "		break;" << endl;
	stream << "	}" << endl;
	stream << "	}" << endl;
	stream << '}' << endl;

	/* End the namespace. */
	EndNamespace(service, stream);
}

/** Open the header file and write the common start.
 * @param service	Service to generate for.
 * @param path		Path to header file.
 * @param stream	Stream to use. Will refer to the header file when the
 *			function returns.
 * @return		Whether successful in opening the header. */
bool CXXCodeGen::BeginHeader(Service *service, const string &path, ofstream &stream) {
	if(!service->GetParent()) {
		/* Open the file. */
		stream.open(path.c_str(), ofstream::trunc);
		if(stream.fail()) {
			cerr << "Failed to create header file `" << path << "'." << endl;
			return false;
		}

		/* Generate the include guard name. */
		string guard = "__KRPCGEN_CXX_HEADER_", name;
		name = service->GetFullName();
		BOOST_FOREACH(char ch, name) {
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
	}

	/* Begin the namespace for the service. */
	BeginNamespace(service, stream);

	/* Now write out definitions for type aliases and structures. */
	BOOST_FOREACH(const Type *type, service->GetTypes()) {
		const AliasType *atype = dynamic_cast<const AliasType *>(type);
		if(atype) {
			stream << "typedef " << GetCXXType(atype->Resolve());
			stream << ' ' << atype->GetName() << ';' << endl;
			continue;
		}

		const StructType *stype = dynamic_cast<const StructType *>(type);
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

	/* Write information for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		BeginHeader(child, path, stream);
	}

	/* End the namespace. */
	EndNamespace(service, stream);
	return true;
}

/** Finish and close the header file.
 * @param stream	Stream for the header. */
void CXXCodeGen::EndHeader(ofstream &stream) {
	stream << "#endif" << endl;
	stream.close();
}

/** Open the output file and write the common start.
 * @param service	Service to generate for.
 * @param path		Path to output file.
 * @param stream	Stream to use. Will refer to the file when the function
 *			returns.
 * @return		Whether successful in opening the file. */
bool CXXCodeGen::BeginCode(Service *service, const string &path, ofstream &stream) {
	if(!service->GetParent()) {
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
	}

	/* Begin the namespace for the service. */
	BeginNamespace(service, stream);

	/* Write code for nested services. */
	BOOST_FOREACH(Service *child, service->GetChildren()) {
		BeginCode(child, path, stream);
	}

	/* Write the struct (un)serialisation functions. */
	BOOST_FOREACH(const Type *type, service->GetTypes()) {
		const StructType *stype = dynamic_cast<const StructType *>(type);
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

	/* End the namespace. */
	EndNamespace(service, stream);
	return true;
}

/** Finish and close the output file.
 * @param stream	Stream for the file. */
void CXXCodeGen::EndCode(ofstream &stream) {
	stream.close();
}

/** Write out the namespace start.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::BeginNamespace(Service *service, ofstream &stream) {
	vector<string> tokens;
	service->TokeniseName(tokens);
	BOOST_FOREACH(string &str, tokens) {
		stream << "namespace " << str << " {" << endl;
	}
}

/** Write out the namespace end.
 * @param service	Service to generate for.
 * @param stream	Stream to write to. */
void CXXCodeGen::EndNamespace(Service *service, ofstream &stream) {
	vector<string> tokens;
	service->TokeniseName(tokens);
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
