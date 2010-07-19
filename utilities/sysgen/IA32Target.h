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
 * @brief		IA32 system call code generator.
 */

#ifndef __IA32TARGET_H
#define __IA32TARGET_H

#include "sysgen.h"

/** C++ code generator class. */
class IA32Target : public Target {
public:
	void addTypes(TypeMap &map);
	size_t maxParameters();
	void generate(std::ostream &stream, const SyscallList &calls, bool use_errno);
};

#endif /* __IA32TARGET_H */
