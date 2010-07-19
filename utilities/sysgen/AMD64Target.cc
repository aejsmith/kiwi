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
 * @brief		AMD64 system call code generator.
 */

#include <boost/foreach.hpp>

#include "AMD64Target.h"

using namespace std;

/** Add this target's types to the type map.
 * @param map		Map to add to. */
void AMD64Target::addTypes(TypeMap &map) {
	map["int"] = new Type(32, 1, true);
	map["char"] = new Type(8, 1, true);
	map["bool"] = new Type(1, 1, false);
	map["ptr_t"] = new Type(64, 1, false);
	map["size_t"] = new Type(64, 1, false);
	map["int8_t"] = new Type(8, 1, true);
	map["int16_t"] = new Type(16, 1, true);
	map["int32_t"] = new Type(32, 1, true);
	map["int64_t"] = new Type(64, 1, true);
	map["uint8_t"] = new Type(8, 1, false);
	map["uint16_t"] = new Type(16, 1, false);
	map["uint32_t"] = new Type(32, 1, false);
	map["uint64_t"] = new Type(64, 1, false);
}

/** Get the maximum number of parameters allowed to a call.
 * @return		Maximum number of parameters. */
size_t AMD64Target::maxParameters() {
	return 6;
}

/** Generate the system call code.
 * @param stream	Stream to write output to.
 * @param calls		List of system calls.
 * @param use_errno	Whether to generate code to set errno. */
void AMD64Target::generate(std::ostream &stream, const SyscallList &calls, bool use_errno) {
	stream << "/* This file is automatically generated. Do not edit! */" << endl;

	BOOST_FOREACH(const Syscall *call, calls) {
		/* The code is the same regardless of parameter count. */
		stream << endl;
		stream << ".global " << call->getName() << endl;
		stream << ".type " << call->getName() << ", @function" << endl;
		stream << call->getName() << ':' << endl;
		stream << "	movq	%rcx, %r10" << endl;
		stream << "	movq	$" << call->getID() << ", %rax" << endl;
		stream << "	syscall" << endl;

		/* If the return type is signed and the function returns a
		 * negative value, set the error number. */
		Type *type = call->getReturnType();
		if(type && type->is_signed && use_errno) {
			if(type->width == 64) {
				stream << "	test	%rax, %rax" << endl;
			} else {
				stream << "	test	%eax, %eax" << endl;
			}
			stream << "	js	1f" << endl;
			stream << "	ret" << endl;
			stream << "1:	movl	%eax, %eax" << endl;
			stream << "	neg	%eax" << endl;
			stream << "	push	%rax" << endl;
			stream << "#ifdef SHARED" << endl;
			stream << "	call	__libsystem_errno_location@PLT" << endl;
			stream << "#else" << endl;
			stream << "	call	__libsystem_errno_location" << endl;
			stream << "#endif" << endl;
			stream << "	pop	%rdx" << endl;
			stream << "	movl	%edx, (%rax)" << endl;
			stream << "	movq	$-1, %rax" << endl;
			stream << "	ret" << endl;
		} else {
			stream << "	ret" << endl;
		}

		stream << ".size " << call->getName() << ", .-" << call->getName() << endl;
	}
}
