#
# Copyright (C) 2009-2015 Alex Smith
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
import builders, image

class BuildManager:
    def __init__(self, host_template, target_template):
        self.envs = []
        self.host_template = host_template
        self.target_template = target_template
        self.libraries = {}

        # Add a reference to ourself to all environments.
        self.AddVariable('_MANAGER', self)

        # Create compile strings that will be added to all environments.
        verbose = ARGUMENTS.get('V') == '1'
        def compile_str(msg):
            return None if verbose else '\033[0;32m%8s\033[0m $TARGET' % (msg)
        def compile_str_func(msg, target, source, env):
            return '\033[0;32m%8s\033[0m %s' % (msg, str(target[0]))

        self.AddVariable('ARCOMSTR',     compile_str('AR'))
        self.AddVariable('ASCOMSTR',     compile_str('ASM'))
        self.AddVariable('ASPPCOMSTR',   compile_str('ASM'))
        self.AddVariable('CCCOMSTR',     compile_str('CC'))
        self.AddVariable('SHCCCOMSTR',   compile_str('CC'))
        self.AddVariable('CXXCOMSTR',    compile_str('CXX'))
        self.AddVariable('SHCXXCOMSTR',  compile_str('CXX'))
        self.AddVariable('YACCCOMSTR',   compile_str('YACC'))
        self.AddVariable('LEXCOMSTR',    compile_str('LEX'))
        self.AddVariable('LINKCOMSTR',   compile_str('LINK'))
        self.AddVariable('SHLINKCOMSTR', compile_str('SHLINK'))
        self.AddVariable('RANLIBCOMSTR', compile_str('RANLIB'))
        self.AddVariable('GENCOMSTR',    compile_str('GEN'))
        self.AddVariable('STRIPCOMSTR',  compile_str('STRIP'))

        if not verbose:
            # Substfile doesn't provide a method to override the output. Hack around.
            func = lambda t, s, e: compile_str_func('GEN', t, s, e)
            self.host_template['BUILDERS']['Substfile'].action.strfunction = func
            self.target_template['BUILDERS']['Substfile'].action.strfunction = func

        # Add builders from builders.py
        self.AddBuilder('LDScript', builders.ld_script_builder)

        # Create the distribution environment and various methods to add data
        # to an image.
        dist = self.CreateBare(name = 'dist', flags = {
            'FILES': [],
            'LINKS': [],
        })
        def add_file_method(env, target, path):
            env['FILES'].append((path, target))
        def add_link_method(env, target, path):
            env['LINKS'].append((path, target))
        dist.AddMethod(add_file_method, 'AddFile')
        dist.AddMethod(add_link_method, 'AddLink')

        # Add image builders.
        dist['BUILDERS']['FSImage'] = image.fs_image_builder
        dist['BUILDERS']['ISOImage'] = image.iso_image_builder

    def __getitem__(self, key):
        """Get an environment by name."""

        for (k, v) in self.envs:
            if k and k == key:
                return v

        return None

    def AddVariable(self, name, value):
        """Add a variable to all environments and all future environments."""

        self.host_template[name] = value
        self.target_template[name] = value

        for (k, v) in self.envs:
            v[name] = value

    def AddBuilder(self, name, builder):
        """Add a builder to all environments and all future environments."""

        self.host_template['BUILDERS'][name] = builder
        self.target_template['BUILDERS'][name] = builder

        for (k, v) in self.envs:
            v['BUILDERS'][name] = builder

    def AddTool(self, name, depends, act):
        """Add a build tool to all environments and all future environments."""

        if type(depends) != list:
            depends = [depends]
        def dep_emitter(target, source, env):
            for dep in depends:
                Depends(target, dep)
            return (target, source)
        self.AddBuilder(name, Builder(action = act, emitter = dep_emitter))

    def AddLibrary(self, name, build_libraries, include_paths):
        self.libraries[name] = {
            'build_libraries': build_libraries,
            'include_paths': include_paths,
        }

    def CreateHost(self, **kwargs):
        """Create an environment for building for the host system."""

        name = kwargs['name'] if 'name' in kwargs else None
        flags = kwargs['flags'] if 'flags' in kwargs else {}

        env = self.host_template.Clone()
        self.merge_flags(env, flags)
        self.envs.append((name, env))
        return env

    def CreateBare(self, **kwargs):
        """Create an environment for building for the target system."""

        name = kwargs['name'] if 'name' in kwargs else None
        flags = kwargs['flags'] if 'flags' in kwargs else {}

        env = self.target_template.Clone()
        self.merge_flags(env, flags)
        self.envs.append((name, env))
        return env

    def Create(self, **kwargs):
        """Create an environment for building for the target system."""

        name = kwargs['name'] if 'name' in kwargs else None
        flags = kwargs['flags'] if 'flags' in kwargs else {}
        libraries = kwargs['libraries'] if 'libraries' in kwargs else []

        env = self.target_template.Clone()
        config = env['_CONFIG']

        # Get the compiler include directory which contains some standard
        # headers.
        from subprocess import Popen, PIPE
        incdir = Popen([env['CC'], '-print-file-name=include'], stdout = PIPE).communicate()[0].strip().decode('utf-8')

        # Specify -nostdinc to prevent the compiler from using the automatically
        # generated sysroot. That only needs to be used when compiling outside
        # the build system, we manage all the header paths internally. We do
        # need to add the compiler's own include directory to the path, though.
        self.merge_flags(env, {
            'ASFLAGS': ['-nostdinc', '-isystem', incdir, '-include',
                'build/%s-%s/config.h' % (config['ARCH'], config['PLATFORM'])],
            'CCFLAGS': ['-nostdinc', '-isystem', incdir, '-include',
                'build/%s-%s/config.h' % (config['ARCH'], config['PLATFORM'])],
            'LIBPATH': [env['_LIBOUTDIR']],
            'LIBS': libraries,
        })

        # Add in specified flags.
        self.merge_flags(env, flags)

        # Add paths for dependencies.
        def add_library(lib):
            if lib in self.libraries:
                paths = [d[0] if type(d) == tuple else d for d in self.libraries[lib]['include_paths']]
                self.merge_flags(env, {'CPPPATH': paths})
                for dep in self.libraries[lib]['build_libraries']:
                    add_library(dep)
        for lib in libraries:
            add_library(lib)

        # Add paths for default libraries. Technically we shouldn't add libc++
        # here if what we're building isn't C++, but we don't know that here,
        # so just add it - it's not a big deal.
        if not 'CCFLAGS' in flags or '-nostdinc' not in flags['CCFLAGS']:
            add_library('system')
            add_library('c++')

        # Set up emitters to set dependencies on default libraries.
        def add_library_deps(target, source, env):
            if not ('-nostdlib' in env['LINKFLAGS'] or '-nostartfiles' in env['LINKFLAGS']):
                Depends(target[0], env['_LIBOUTDIR'].glob('*crt*.o'))
            if not ('-nostdlib' in env['LINKFLAGS'] or '-nodefaultlibs' in env['LINKFLAGS']):
                Depends(target[0], env['_LIBOUTDIR'].File('libsystem.so'))
                if env['SMARTLINK'](source, target, env, None) == '$CXX':
                    Depends(target[0], env['_LIBOUTDIR'].File('libc++.so'))
            return target, source
        env.Append(SHLIBEMITTER = [add_library_deps])
        env.Append(PROGEMITTER = [add_library_deps])

        # Add the application/library builders.
        env.AddMethod(builders.kiwi_application_method, 'KiwiApplication')
        env.AddMethod(builders.kiwi_library_method, 'KiwiLibrary')

        self.envs.append((name, env))
        return env

    def Clone(self, base, **kwargs):
        """Create a new environment based on an existing named environment."""

        name = kwargs['name'] if 'name' in kwargs else None
        flags = kwargs['flags'] if 'flags' in kwargs else {}

        env = base.Clone()
        self.merge_flags(env, flags)
        self.envs.append((name, env))
        return env

    def merge_flags(self, env, flags):
        # The MergeFlags function in Environment only handles lists. Add
        # anything else manually.
        merge = {}
        for (k, v) in flags.items():
            if type(v) == list:
                if k in env:
                    merge[k] = v
                else:
                    env[k] = v
            elif type(v) == dict and k in env and type(env[k]) == dict:
                env[k].update(v)
            else:
                env[k] = v
        env.MergeFlags(merge)
