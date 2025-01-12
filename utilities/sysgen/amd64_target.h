/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 system call code generator.
 */

#ifndef SYSGEN_AMD64_TARGET_H
#define SYSGEN_AMD64_TARGET_H

#include "sysgen.h"

/** AMD64 target class. */
class AMD64Target : public Target {
public:
    void add_types(TypeMap &map);
    void generate(std::ostream &stream, const SyscallList &calls);
};

#endif /* SYSGEN_AMD64_TARGET_H */
