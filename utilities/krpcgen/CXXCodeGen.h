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

#ifndef __CXXCODEGEN_H
#define __CXXCODEGEN_H

#include <fstream>
#include "CodeGen.h"

/** C++ code generator class. */
class CXXCodeGen : public CodeGen {
public:
	bool Generate(Service *service, const std::string &path, bool server, bool client);

	/** Get the output file extension.
	 * @return		Output file extension. */
	const char *OutputExtension() const { return ".cc"; }
private:
	void GenerateServerHeader(Service *service, std::ofstream &stream);
	void GenerateServerCode(Service *service, std::ofstream &stream);
	void GenerateClientHeader(Service *service, std::ofstream &stream);
	void GenerateClientCode(Service *service, std::ofstream &stream);

	bool BeginHeader(Service *service, const std::string &path, std::ofstream &stream);
	void EndHeader(std::ofstream &stream);
	bool BeginCode(Service *service, const std::string &path, std::ofstream &stream);
	void EndCode(std::ofstream &stream);

	void BeginNamespace(Service *service, std::ofstream &stream);
	void EndNamespace(Service *service, std::ofstream &stream);

	std::string GetHeaderPath(const std::string &fpath);
	std::string GetCXXType(Type *type);
	std::string GetFunctionParams(const Function *func);
	std::string GetEventParams(const Function *event);
	std::string GetHandlerCall(const Function *func);
};

#endif /* __CXXCODEGEN_H */
