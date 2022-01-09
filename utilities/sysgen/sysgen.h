/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               System call code generator.
 */

#ifndef SYSGEN_H
#define SYSGEN_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

#include <iostream>
#include <list>
#include <map>
#include <string>

/** Structure representing a type. */
struct Type {
    size_t count;                   /**< Number of parameters this type takes. */
public:
    Type(size_t c = 0) : count(c) {}
};

/** Type of the type map. */
typedef std::map<std::string, Type> TypeMap;

/** Class representing a system call. */
class Syscall {
public:
    /** Attributes for a call. */
    enum {
        kHiddenAttribute = (1 << 0),
        kWrappedAttribute = (1 << 1),
    };
public:
    Syscall(const char *name, unsigned long id) :
        m_name(name), m_id(id), m_num_params(0), m_attributes(0)
    {}

    /** Add a parameter to the call.
     * @param type          Type of the parameter. */
    void add_param(Type type) {
        m_num_params += type.count;
    }

    /** Set an attribute.
     * @param attribute     Attribute to set. */
    void set_attribute(unsigned attribute) {
        m_attributes |= attribute;

        if (attribute == kWrappedAttribute) {
            /* Wrapped implies hidden, as the real call version should not be
             * visible. */
            m_attributes |= kHiddenAttribute;
        }
    }

    /** Get the name of the call.
     * @return              Reference to call name. */
    const std::string &name() const { return m_name; }

    /** Get the ID of the call.
     * @return              ID of the function. */
    unsigned long id() const { return m_id; }

    /** Get the parameter count.
     * @return              Number of parameters. */
    size_t num_params() const { return m_num_params; }

    /** Get the call attributes.
     * @return              Attributes for the call. */
    unsigned attributes() const { return m_attributes; }
private:
    std::string m_name;             /**< Name of the call. */
    unsigned long m_id;             /**< ID of the call. */
    size_t m_num_params;            /**< Number of parameters. */
    unsigned m_attributes;          /**< Attributes for the call. */
};

/** Type of a system call list. */
typedef std::list<Syscall *> SyscallList;

/** Base class for a code generation target. */
class Target {
public:
    virtual ~Target() {}

    /** Add the target's basic types to the type map.
     * @param map           Map to add to. */
    virtual void add_types(TypeMap &map) = 0;

    /** Generate system call functions.
     * @param stream        Stream to write to.
     * @param calls         List of calls to generate. */
    virtual void generate(std::ostream &stream, const SyscallList &calls) = 0;
};

extern "C" {

#endif /* __cplusplus */

/** Structure used to represent an identifier during parsing. */
typedef struct identifier {
    struct identifier *next;        /**< Next identifier in the list. */
    char *str;                      /**< Identifier. */
} identifier_t;

extern FILE *yyin;
extern const char *current_file;
extern size_t current_line;

extern identifier_t *new_identifier(const char *str, identifier_t *next);
extern void add_type(const char *name, const char *target);
extern void add_syscall(const char *name, identifier_t *params, identifier_t *attribs, long num);

extern int yylex(void);
extern int yyparse(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSGEN_H */
