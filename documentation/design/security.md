Security Model
==============

License
-------

This document is distributed under the terms of the [Creative Commons
Attribution-Noncommercial-Share Alike 3.0 Unported](http://creativecommons.org/
licenses/by-nc-sa/3.0/) license.

Introduction
------------

This document describes the security model implemented by Kiwi to control
access to objects and what processes are allowed to do. It describes security
contexts, system-wide privileges, and the methods used by kernel object types
to control access to them.

Security Contexts
-----------------

The core part of the security model is the security context. A security context
specifies an identity and a set of privileges (see next section). Each process
has a security context associated with it. In addition to the process-wide
security context, each thread can set its own security context, which overrides
the process-wide context. This can be used to temporarily change identity, or
drop privileges.

An identity is a single user ID and a set of group IDs. The only knowledge of
users and groups that the kernel has is user and group IDs. User/group names,
logging in, etc. are handled in userspace. User and group IDs also do not have
any special meaning to the kernel, i.e. unlike UNIX, having user ID 0 does not
automatically grant extra privileges. Instead, the privileges mechanism (see
the next section) is used to grant additional privileges to processes,
regardless of their user ID.

A security token is a kernel object which encapsulates a security context. The
process/thread security context is set by specifying a handle to a token object
containing the new context. Security tokens are immutable, i.e. once created,
the context held cannot be modified. The kernel provides APIs to create a token
object from the current process-wide security context, the current per-thread
context (if any), or a supplied context structure. A thread cannot create a
token granting privileges that it does not itself have, and cannot set a
different identity to its current identity unless it has the
`PRIV_CHANGE_IDENTITY` privilege.

Security tokens are transferrable objects. This means that a thread can grant
its privileges to another process by creating a token and then sending a handle
to it over an IPC connection.

Privileges
----------

Privileges are used for system-wide access control, such as restricting the use
of kernel operations that do not operate on an object (shutdown/reboot, loading
modules, etc). They also influence the security rules for object access.

A security context has two sets of privileges: effective and inheritable. The
effective set is the one actually used for privilege checks. The inheritable
set is a subset of the effective set, which will be set as both the effective
and inheritable set on any created child processes.

Object Security
---------------

Some object types, for example timers, are not globally accessible, and only
accessible by the process that creates them or any process which the creator
passes a handle to. These local objects require no access control. Other object
types such as processes, threads and files, however, are globally accessible.
These implement access control based on the accessing thread's security
context. The sections below describe the access control mechanisms implemented
by various object types.

### Files

Files implement the most complex access control system of any object type.
Access to files is controlled by an access control list (ACL) which grants
rights based on the identity of a thread. An ACL can contain entries (ACEs,
access control entries) of the following type:

 * **User** - Specifies rights for a user.
 * **Group** - Specifies rights for all users in a group.
 * **Others** - Specifies rights for threads that do not match any user or
   group entries.
 * **Privilege** - Specifies rights for threads with a certain privilege.
 * **Everyone** - Specifies rights granted to everyone.

When opening a handle to a file, a set of requested rights must be specified.
The file's ACL is checked against the thread's current security context to
determine whether the rights are allowed to the thread. The following rules
determine the set of rights a thread is allowed for an file based on its
identity:

 * If a user entry matches the thread's user ID, the rights it specifies will
   be added to the set of allowed rights.
 * Otherwise, if any group entries match any of the groups the thread is in,
   the rights specified by each matching entry will be added to the set of
   allowed rights.
 * Otherwise, if an others entry exists, the rights it specifies will be added
   to the set of allowed rights.

In addition to this:

 * If privilege entries exist for any of the process' privileges, the rights
   specified by each matching entry will be added to the set of allowed rights.
 * If an everyone entry exists, the rights it specifies will be added to the
   set of allowed rights.

If any of the requested rights are not in the calculated set of rights allowed
to the thread, the open call is denied. Otherwise, the handle is created with
the requested rights. This means that the ACL is only checked at open time, and
operations on a file handle only need to check if the handle has the necessary
rights.

### Processes/Threads

Access to a process or thread object is determined by the security context of
the thread making the access and the security context of the target _process_
(_not_ the target thread). If accessing a thread and the thread has an
overridden security context, this is ignored. Access is determined based on the
process-wide context of its owning process is used.

### IPC

See IPC documentation for more details.
