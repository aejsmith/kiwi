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

#ifndef __CXXCODEGEN_H
#define __CXXCODEGEN_H

#include <fstream>
#include "CodeGen.h"

/** C++ code generator class. */
class CXXCodeGen : public CodeGen {
public:
	CXXCodeGen(Service *service);

	bool GenerateServer(const std::string &path);
	bool GenerateClient(const std::string &path);
private:
	bool GenerateServerHeader(const std::string &path);
	bool GenerateServerCode(const std::string &path);
	bool GenerateClientHeader(const std::string &path);
	bool GenerateClientCode(const std::string &path);

	bool BeginHeader(const std::string &path, std::ofstream &stream);
	void EndHeader(std::ofstream &stream);
	bool BeginCode(const std::string &path, std::ofstream &stream);
	void EndCode(std::ofstream &stream);

	void StartNamespace(std::ofstream &stream);
	void EndNamespace(std::ofstream &stream);

	std::string GetHeaderPath(const std::string &fpath);
	std::string GetCXXType(Type *type);
	std::string GetFunctionParams(const Function *func);
	std::string GetEventParams(const Function *event);
	std::string GetHandlerCall(const Function *func);
};

#endif /* __CXXCODEGEN_H */
