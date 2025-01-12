/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 system call code generator.
 */

#ifndef SYSGEN_ARM64_TARGET_H
#define SYSGEN_ARM64_TARGET_H

#include "sysgen.h"

/** ARM64 target class. */
class ARM64Target : public Target {
public:
    void add_types(TypeMap &map);
    void generate(std::ostream &stream, const SyscallList &calls);
};

#endif /* SYSGEN_ARM64_TARGET_H */
