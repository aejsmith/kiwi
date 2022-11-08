#
# Copyright (C) 2009-2022 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

from SCons.Script import *
import os

# Builder to pre-process a linker script
ld_script_builder = Builder(action = Action(
    '$CC $_CCCOMCOM $ASFLAGS -E -x c $SOURCE | grep -v "^\#" > $TARGET',
    '$GENCOMSTR'))

# Custom method to build a Kiwi application.
#
# name            = Name of the target.
# sources         = Source files.
# override_flags  = Flags to pass to the SCons SharedLibrary builder, which
#                   completed replace the environment flags it contains.
def kiwi_application_method(env, name, sources, **kwargs):
    override_flags = kwargs['override_flags'] if 'override_flags' in kwargs else {}

    target = File(name)

    # Add the application to the image.
    dist = env['MANAGER']['dist']
    dist.AddFile(target, 'system/bin/%s' % (name))

    # Build the application.
    return env.Program(target, sources, **override_flags)

# Custom method to build a Kiwi service.
#
# name            = Name of the target.
# sources         = Source files.
# override_flags  = Flags to pass to the SCons SharedLibrary builder, which
#                   completed replace the environment flags it contains.
def kiwi_service_method(env, name, sources, **kwargs):
    override_flags = kwargs['flags'] if 'flags' in kwargs else {}

    target = File(name)

    # Add the application to the image.
    dist = env['MANAGER']['dist']
    dist.AddFile(target, 'system/services/%s' % (name))

    # Build the application.
    return env.Program(target, sources, **override_flags)

def lib_links_action(target, source, env):
    for link in env['LIBLINKS']:
        path = os.path.join(str(target[0].dir), link)
        try:
            os.remove(path)
        except:
            pass
        os.symlink(target[0].name, path)
    return 0

# Custom method to build a Kiwi library.
#
# name            = Name of the library.
# sources         = Source files.
# version         = Shared library version, "<major>.<minor>.<revision>". Only
#                   major is needed (and in general, we only use major for our
#                   own libraries). If not specified, defaults to "1". Set to
#                   None explicitly to create an unversioned library.
# build_libraries = Other libraries required for building against this library.
# include_paths   = List of include paths for the library. Each entry is
#                   either a SCons Dir instance, or a tuple of
#                   (Dir, sysroot location). The tuple form allows installing
#                   the directory to a different location in the sysroot.
# override_flags  = Flags to pass to the SCons SharedLibrary builder, which
#                   completely replace the environment flags it contains.
def kiwi_library_method(env, name, sources, **kwargs):
    manager = env['MANAGER']

    version         = kwargs['version'] if 'version' in kwargs else '1'
    build_libraries = kwargs['build_libraries'] if 'build_libraries' in kwargs else []
    include_paths   = kwargs['include_paths'] if 'include_paths' in kwargs else []
    override_flags  = kwargs['override_flags'] if 'override_flags' in kwargs else {}

    # Register this library with the build manager.
    manager.add_library(name, build_libraries, include_paths)

    # Get library file name with version, and the links to make. We create a
    # link with the unversioned name, and a link with the major version if the
    # version has more than the major component.
    links = []
    if version != None:
        file   = 'lib%s.so.%s' % (name, version)
        parts  = version.split('.')
        soname = 'lib%s.so.%s' % (name, parts[0])
        if len(parts) > 1:
            links.append('lib%s.so.%s' % (name, parts[0]))
        links.append('lib%s.so' % (name))
    else:
        file   = 'lib%s.so' % (name)
        soname = file

    # Modify the target path so that libraries all get placed in the build
    # library directory. This is necessary for default libraries (libkernel,
    # libsystem, etc.) so that they can be found by the linker.
    out_dir = str(env['_LIBOUTDIR'])
    target  = File('%s/%s' % (out_dir, file))

    # Add the library to the distribution environment.
    dist = manager['dist']
    dist.AddFile(target, 'system/lib/%s' % (file))
    for link in links:
        dist.AddLink(file, 'system/lib/%s' % (link))

    # Add the library and its includes to the sysroot manifest. Don't need
    # tracking on the includes.
    sysroot = manager['sysroot']
    sysroot.AddFile(target, 'lib/%s' % (file))
    for link in links:
        sysroot.AddLink(file, 'lib/%s' % (link))
    for path in include_paths:
        if isinstance(path, tuple):
            source_path = str(path[0].srcnode())
            dest_path   = 'include/%s' % (path[1])
        else:
            source_path = str(path.srcnode())
            dest_path   = 'include'
        sysroot['MANIFEST'].add_from_dir_tree(
            source_path = source_path, dest_path = dest_path, tracked = False,
            follow_links = True)

    override_flags['SONAME'] = soname
    override_flags['LIBLINKS'] = links

    # Build the library.
    node = env.SharedLibrary(target, sources, **override_flags)

    if links:
        env.AddPostAction(node, env.Action(lib_links_action, None))
        for link in links:
            env.SideEffect(File('%s/%s' % (out_dir, link)), node)

    return node
