#
# Copyright (C) 2009-2020 Alex Smith
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
def kiwi_application_method(env, name, sources, **kwargs):
    flags = kwargs['flags'] if 'flags' in kwargs else {}

    target = File(name)

    # Add the application to the image.
    dist = env['_MANAGER']['dist']
    dist.AddFile(target, 'system/bin/%s' % (name))

    # Build the application.
    return env.Program(target, sources, **flags)

# Custom method to build a Kiwi library.
def kiwi_library_method(env, name, sources, **kwargs):
    manager = env['_MANAGER']

    build_libraries = kwargs['build_libraries'] if 'build_libraries' in kwargs else []
    include_paths = kwargs['include_paths'] if 'include_paths' in kwargs else []
    flags = kwargs['flags'] if 'flags' in kwargs else {}

    # Register this library with the build manager.
    manager.AddLibrary(name, build_libraries, include_paths)

    # Modify the target path so that libraries all get placed in the build
    # library directory.
    target = File('%s/lib%s.so' % (str(env['_LIBOUTDIR']), name))

    # Add the library to the distribution environment.
    dist = manager['dist']
    dist.AddFile(target, 'system/lib/lib%s.so' % (name))

    # Build the library.
    return env.SharedLibrary(target, sources, **flags)
