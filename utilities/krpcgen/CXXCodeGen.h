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

	bool generateServer(const std::string &path);
	bool generateClient(const std::string &path);
private:
	bool generateServerHeader(const std::string &path);
	bool generateServerCode(const std::string &path);
	bool generateClientHeader(const std::string &path);
	bool generateClientCode(const std::string &path);

	bool beginHeader(const std::string &path, std::ofstream &stream);
	void endHeader(std::ofstream &stream);
	bool beginCode(const std::string &path, std::ofstream &stream);
	void endCode(std::ofstream &stream);

	void startNamespace(std::ofstream &stream);
	void endNamespace(std::ofstream &stream);

	std::string getHeaderPath(const std::string &fpath);
	std::string getCXXType(Type *type);
	std::string getFunctionParams(const Function *func);
	std::string getEventParams(const Function *event);
	std::string getHandlerCall(const Function *func);
};

#endif /* __CXXCODEGEN_H */
