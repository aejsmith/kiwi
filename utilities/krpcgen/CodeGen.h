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
 * @brief		RPC code generation class.
 */

#ifndef __CODEGEN_H
#define __CODEGEN_H

#include <string>
#include "Service.h"

/** Base class for a code generator. */
class CodeGen {
public:
	CodeGen(Service *service) : m_service(service) {}

	/** Generate server code.
	 * @param path		Path to output file.
	 * @return		Whether generated successfully. */
	virtual bool GenerateServer(const std::string &path) = 0;

	/** Generate client code.
	 * @param path		Path to output file.
	 * @return		Whether generated successfully. */
	virtual bool GenerateClient(const std::string &path) = 0;
protected:
	Service *m_service;		/**< Service to generate for. */
};

#endif /* __CODEGEN_H */
