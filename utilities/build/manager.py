#
# Copyright (C) 2009-2023 Alex Smith
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
import builders
import image
import manifest
import os
import subprocess

###############
# Build flags #
###############

# C/C++ warning flags.
cc_warning_flags = [
    '-Wall', '-Wextra', '-Wno-variadic-macros', '-Wno-unused-parameter',
    '-Wwrite-strings', '-Wmissing-declarations', '-Wredundant-decls',
    '-Werror', '-Wno-error=unused',
]

# C++ warning flags.
cxx_warning_flags = [
    '-Wsign-promo',
]

# Variables to set in target environments.
#
# TODO: -fno-omit-frame-pointer should really be restricted to kernel builds
# but for now we use it everywhere since that's all we have for doing
# backtraces.
target_flags = {
    'CCFLAGS': cc_warning_flags + ['-pipe', '-fno-omit-frame-pointer'],
    'CFLAGS': ['-std=gnu11'],
    'CXXFLAGS': cxx_warning_flags + ['-std=c++20'],
    'ASFLAGS': ['-D__ASM__'],
}

# Per-build-type target flags.
target_type_flags = {
    'debug': {
        'CCFLAGS': ['-gdwarf-2', '-O0'],
    },
    'debugopt': {
        'CCFLAGS': ['-gdwarf-2', '-O2'],
    },
    'release': {
        'CCFLAGS': ['-O2'],
    },
}

# Variables to set in host environments. Remove flags unsupported by some older
# host G++ versions.
host_flags = {
    'CCFLAGS': ['-pipe'] + [f for f in cc_warning_flags if f not in [
        '-Wmissing-declarations', '-Wno-variadic-macros',
        '-Wno-unused-but-set-variable']],
    'CFLAGS': ['-std=gnu99'],
    'CXXFLAGS': ['-std=c++17'],
    'YACCFLAGS': ['-d'],
}

#################
# Build manager #
#################

class BuildManager:
    def __init__(self, config):
        self.envs = []
        self.host_template = Environment(ENV = os.environ, tools = ['default', 'textfile'])
        self.target_template = Environment(platform = 'posix', ENV = os.environ, tools = ['default', 'textfile'])
        self.libraries = {}
        self.verbose = ARGUMENTS.get('V') == '1'
        self.config = config
        self.target_build_dir = os.path.join('build', '%s-%s' % (self.config['ARCH'], self.config['BUILD']))

        # Add a reference to ourself to all environments.
        self.add_variable('MANAGER', self)
        self.add_variable('CONFIG', config)

        self._init_output()
        self._init_tools()
        self._init_host()
        self._init_dist()
        self._init_sysroot()

    # Create compile strings that will be added to all environments.
    def _init_output(self):
        self.add_variable('ARCOMSTR',     self.compile_str('AR'))
        self.add_variable('ASCOMSTR',     self.compile_str('ASM'))
        self.add_variable('ASPPCOMSTR',   self.compile_str('ASM'))
        self.add_variable('CCCOMSTR',     self.compile_str('CC'))
        self.add_variable('SHCCCOMSTR',   self.compile_str('CC'))
        self.add_variable('CXXCOMSTR',    self.compile_str('CXX'))
        self.add_variable('SHCXXCOMSTR',  self.compile_str('CXX'))
        self.add_variable('YACCCOMSTR',   self.compile_str('YACC'))
        self.add_variable('LEXCOMSTR',    self.compile_str('LEX'))
        self.add_variable('LINKCOMSTR',   self.compile_str('LINK'))
        self.add_variable('SHLINKCOMSTR', self.compile_str('SHLINK'))
        self.add_variable('RANLIBCOMSTR', self.compile_str('RANLIB'))
        self.add_variable('GENCOMSTR',    self.compile_str('GEN'))
        self.add_variable('STRIPCOMSTR',  self.compile_str('STRIP'))

        if not self.verbose:
            # Substfile doesn't provide a method to override the output. Hack around.
            func = lambda t, s, e: self.compile_str_func('GEN', t, s, e)
            self.host_template['BUILDERS']['Substfile'].action.strfunction = func
            self.target_template['BUILDERS']['Substfile'].action.strfunction = func

    # Add tools/builders common to all environments.
    def _init_tools(self):
        self.add_builder('LDScript', builders.ld_script_builder)

        self.host_template.Tool('compilation_db')
        self.target_template.Tool('compilation_db')

        # Setting COMPILATION_DBCOMSTR to None or empty results in the function
        # name being printed, this silences it completely.
        self.host_template.CompilationDatabase.builder.action.cmdstr = None
        self.target_template.CompilationDatabase.builder.action.cmdstr = None

    # Initialise the host environment template.
    def _init_host(self):
        # Set up the host environment template.
        for (k, v) in host_flags.items():
            self.host_template[k] = v

        # Darwin hosts probably have needed libraries in /opt.
        if os.uname()[0] == 'Darwin':
            self.host_template['CPPPATH'] = ['/opt/local/include']
            self.host_template['LIBPATH'] = ['/opt/local/lib']

    # Create the distribution environment.
    def _init_dist(self):
        dist = self.create_bare(name = 'dist', flags = {
            'MANIFEST': manifest.Manifest(),
        })

        dist.AddMethod(manifest.add_file_method, 'AddFile')
        dist.AddMethod(manifest.add_link_method, 'AddLink')
        dist.AddMethod(manifest.manifest_method, 'Manifest')
        dist.AddMethod(image.fs_archive_method, 'FSArchive')
        dist.AddMethod(image.boot_archive_method, 'BootArchive')
        dist.AddMethod(image.iso_image_method, 'ISOImage')
        dist.AddMethod(image.disk_image_method, 'DiskImage')

    # Create an environment for building the sysroot.
    def _init_sysroot(self):
        sysroot = self.create_bare(name = 'sysroot', flags = {
            'MANIFEST': manifest.Manifest(),
        })

        sysroot.AddMethod(manifest.add_file_method, 'AddFile')
        sysroot.AddMethod(manifest.add_link_method, 'AddLink')
        sysroot.AddMethod(manifest.manifest_method, 'Manifest')

    # Initialise the target environment template.
    def init_target(self, toolchain):
        for (k, v) in target_flags.items():
            self.target_template[k] = v
        for (k, v) in target_type_flags[self.config['BUILD']].items():
            self.target_template[k] += v

        # TODO: Enable FPU support for ARM64
        if self.config['ARCH'] == 'arm64':
            self.target_template['CCFLAGS'] += ['-mgeneral-regs-only']

        # We don't use the toolchain generated "<target>-clang" wrappers, as
        # this sets the sysroot to the external one, which isn't always set up.
        # We want to use our internal build tree only. Make wrappers to use
        # during build.
        build_sysroot_dir   = os.path.join(os.getcwd(), self.target_build_dir)
        build_toolchain_dir = os.path.join(os.getcwd(), 'build', 'toolchain')
        os.makedirs(build_toolchain_dir, exist_ok = True)
        def tool_wrapper_path(name):
            return os.path.join(build_toolchain_dir, '%s-%s' % (toolchain.target, name))
        for name in ['clang', 'clang++']:
            path    = toolchain.generic_tool_path(name)
            wrapper = tool_wrapper_path(name)
            with open(wrapper, 'w') as f:
                f.write('#!/bin/bash\n\n')
                f.write('exec -a "$0" "%s" --sysroot="%s" "$@"\n' % (path, build_sysroot_dir))
            os.chmod(wrapper, 0o755)

        # Clang's integrated assembler doesn't support 16-bit code.
        self.target_template['ASFLAGS'] = ['-D__ASM__', '-no-integrated-as']

        # Set correct shared library link flags.
        self.target_template['SHCCFLAGS']   = '$CCFLAGS -fPIC -DSHARED'
        self.target_template['SHLINKFLAGS'] = '$LINKFLAGS -shared -Wl,-soname,${TARGET.name}'

        # Override default assembler - it uses as directly, we want to go
        # through the compiler.
        self.target_template['ASCOM'] = '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES'

        # Add an action for ASM files in a shared library, and emitters that
        # handle -include arguments.
        from SCons.Tool import createObjBuilders
        static_obj, shared_obj = createObjBuilders(self.target_template)
        shared_obj.add_action('.S', Action('$CC $_CCCOMCOM $ASFLAGS -DSHARED -c -o $TARGET $SOURCES', '$ASCOMSTR'))
        def cmdline_deps_emitter(target, source, env):
            for idx, flag in enumerate(env['CCFLAGS']):
                if flag == '-include':
                    next = env.subst(env['CCFLAGS'][idx + 1], conv = lambda x: x)
                    Depends(target[0], File(next))
            return (target, source)
        suffixes = ['.c', '.cpp', '.cxx', '.cc', '.S']
        for suffix in suffixes:
            for builder in [static_obj, shared_obj]:
                emitter = builder.emitter.get(suffix, False)
                builder.emitter[suffix] = SCons.Builder.ListEmitter([emitter, cmdline_deps_emitter])

        # Add in extra compilation flags from the configuration.
        if 'ARCH_ASFLAGS' in self.config:
            self.target_template['ASFLAGS'] += self.config['ARCH_ASFLAGS'].split()
        if 'ARCH_CCFLAGS' in self.config:
            self.target_template['CCFLAGS'] += self.config['ARCH_CCFLAGS'].split()

        self.target_template['CCFLAGS']  += self.config['EXTRA_CCFLAGS'].split()
        self.target_template['CFLAGS']   += self.config['EXTRA_CFLAGS'].split()
        self.target_template['CXXFLAGS'] += self.config['EXTRA_CXXFLAGS'].split()

        # Set paths to toolchain components.
        if 'CC' in os.environ and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
            self.target_template['CC'] = os.environ['CC']
            self.target_template['ENV']['CCC_CC'] = toolchain.tool_path('clang')

            # Force a rebuild when doing static analysis.
            def decide_if_changed(dependency, target, prev_ni):
                return True
            self.target_template.Decider(decide_if_changed)
        else:
            self.target_template['CC'] = tool_wrapper_path('clang')
        if 'CXX' in os.environ and os.path.basename(os.environ['CXX']) == 'c++-analyzer':
            self.target_template['CXX'] = os.environ['CXX']
            self.target_template['ENV']['CCC_CXX'] = tool_wrapper_path('clang++')
        else:
            self.target_template['CXX'] = tool_wrapper_path('clang++')
        self.target_template['AS']      = toolchain.tool_path('as')
        self.target_template['OBJDUMP'] = toolchain.tool_path('objdump')
        self.target_template['READELF'] = toolchain.tool_path('readelf')
        self.target_template['NM']      = toolchain.tool_path('nm')
        self.target_template['STRIP']   = toolchain.tool_path('strip')
        self.target_template['AR']      = toolchain.tool_path('ar')
        self.target_template['RANLIB']  = toolchain.tool_path('ranlib')
        self.target_template['OBJCOPY'] = toolchain.tool_path('objcopy')
        self.target_template['LD']      = toolchain.tool_path('ld')

        # Get the compiler include directory which contains some standard
        # headers.
        toolchain_cmd = [self.target_template['CC'], '-print-file-name=include']
        with subprocess.Popen(toolchain_cmd, stdout = subprocess.PIPE) as proc:
            self.target_template['TOOLCHAIN_INCLUDE'] = proc.communicate()[0].decode('utf-8').strip()

    # Add a variable to all environments and all future environments.
    def add_variable(self, name, value):
        self.host_template[name] = value
        self.target_template[name] = value

        for (k, v) in self.envs:
            v[name] = value

    # Add a builder to all environments and all future environments.
    def add_builder(self, name, builder):
        self.host_template['BUILDERS'][name] = builder
        self.target_template['BUILDERS'][name] = builder

        for (k, v) in self.envs:
            v['BUILDERS'][name] = builder

    # Add a build tool to all environments and all future environments.
    def add_tool(self, name, depends, act):
        if type(depends) != list:
            depends = [depends]
        def dep_emitter(target, source, env):
            for dep in depends:
                Depends(target, dep)
            return (target, source)
        self.add_builder(name, Builder(action = act, emitter = dep_emitter))

    # Add a record of a library that can be referenced when creating an
    # environment. See builders.kiwi_library_method().
    def add_library(self, name, build_libraries, include_paths, lib_paths = []):
        self.libraries[name] = {
            'build_libraries': build_libraries,
            'include_paths': include_paths,
            'lib_paths': lib_paths,
        }

    def _add_config_h(self, env):
        env['CONFIG_H'] = File('#%s/config.h' % (self.target_build_dir))
        self.merge_flags(env, {
            'ASFLAGS': ['-include', '${CONFIG_H}'],
            'CCFLAGS': ['-include', '${CONFIG_H}'],
        })

    # Create an environment for building for the host system.
    def create_host(self, **kwargs):
        name  = kwargs.get('name', None)
        flags = kwargs.get('flags', {})

        env = self.host_template.Clone()
        self.merge_flags(env, flags)
        self.envs.append((name, env))
        return env

    # Create an environment for building for the target system.
    def create_bare(self, **kwargs):
        name          = kwargs.get('name', None)
        flags         = kwargs.get('flags', {})
        with_config_h = kwargs.get('with_config_h', False)

        env = self.target_template.Clone()

        if with_config_h:
            self._add_config_h(env)

        self.merge_flags(env, flags)

        self.envs.append((name, env))
        return env

    # Create an environment for building for the target system.
    def create(self, **kwargs):
        name          = kwargs.get('name', None)
        flags         = kwargs.get('flags', {})
        libraries     = kwargs.get('libraries', [])
        with_config_h = kwargs.get('with_config_h', True)

        env = self.target_template.Clone()

        if with_config_h:
            self._add_config_h(env)

        self.merge_flags(env, {
            # Specify -nostdinc to prevent the compiler from using the
            # automatically generated sysroot. That only needs to be used when
            # compiling outside the build system, we manage all the header
            # paths internally. We do need to add the compiler's own include
            # directory to the path, though.
            'ASFLAGS': ['-nostdinc', '-isystem', env['TOOLCHAIN_INCLUDE']],
            'CCFLAGS': ['-nostdinc', '-isystem', env['TOOLCHAIN_INCLUDE']],

            # Link to the specified libraries and set up the library path.
            'LIBS': libraries,
            'LIBPATH': [env['_LIBOUTDIR']],
        })

        # Add in caller-specified flags.
        self.merge_flags(env, flags)

        # Add paths for library dependencies.
        def add_library_flags(lib, is_build = False):
            if lib in self.libraries:
                lib_spec = self.libraries[lib]

                self.merge_flags(env, {
                    'CPPPATH': [d[0] if isinstance(d, tuple) else d for d in lib_spec['include_paths']],
                })

                # If it's a direct library dependency, add to LIBPATH so that
                # the linker can find it. For indirect dependencies, we need to
                # specify '-rpath-link' to the linker so that it knows where to
                # find the libraries used by the library we're linking to - it
                # does not search paths specified with -L for this.
                for lib_path in lib_spec['lib_paths']:
                    if is_build:
                        self.merge_flags(env, {'LINKFLAGS': ['-Wl,-rpath-link=%s' % (str(lib_path))]})
                    else:
                        self.merge_flags(env, {'LIBPATH': [lib_path]})

                for dep in self.libraries[lib]['build_libraries']:
                    add_library_flags(dep, True)
        for lib in libraries:
            add_library_flags(lib)

        # Add paths for default libraries. Technically we shouldn't add libc++
        # here if what we're building isn't C++, but we don't know that here,
        # so just add it - it's not a big deal.
        if not 'CCFLAGS' in flags or '-nostdinc' not in flags['CCFLAGS']:
            add_library_flags('c++')
            add_library_flags('m')
            add_library_flags('system')

        # Set up emitters to set dependencies on default libraries.
        def add_library_deps(target, source, env):
            if not '-nostdlib' in env['LINKFLAGS']:
                Depends(target[0], env['_LIBOUTDIR'].File('libclang_rt.builtins-%s.a' % (env['CONFIG']['TOOLCHAIN_ARCH'])))
            if not ('-nostdlib' in env['LINKFLAGS'] or '-nostartfiles' in env['LINKFLAGS']):
                Depends(target[0], env['_LIBOUTDIR'].glob('*crt*.o'))
            if not ('-nostdlib' in env['LINKFLAGS'] or '-nodefaultlibs' in env['LINKFLAGS']):
                Depends(target[0], env['_LIBOUTDIR'].File('libsystem.so.1'))
                Depends(target[0], env['_LIBOUTDIR'].File('libkernel.so'))
                if env['SMARTLINK'](source, target, env, None) == '$CXX':
                    Depends(target[0], env['_LIBOUTDIR'].File('libc++.so.1'))
            return target, source
        env.Append(SHLIBEMITTER = [add_library_deps])
        env.Append(PROGEMITTER = [add_library_deps])

        # Add the userspace builders.
        env.AddMethod(builders.kiwi_application_method, 'KiwiApplication')
        env.AddMethod(builders.kiwi_library_method, 'KiwiLibrary')
        env.AddMethod(builders.kiwi_service_method, 'KiwiService')

        self.envs.append((name, env))
        return env

    # Create a new environment based on an existing environment.
    def clone(self, base, **kwargs):
        name  = kwargs.get('name', None)
        flags = kwargs.get('flags', {})

        env = base.Clone()
        self.merge_flags(env, flags)
        self.envs.append((name, env))
        return env

    # Merge flags into an envirornment.
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

    def compile_str(self, msg):
        return None if self.verbose else '\033[0;32m%8s\033[0m $TARGET' % (msg)

    def compile_str_func(self, msg, target, source, env):
        return '\033[0;32m%8s\033[0m %s' % (msg, str(target[0]))

    def compile_log(self, method, prefix, name):
        if self.verbose:
            print('%s("%s")' % (method, name))
        else:
            print('\033[0;32m%8s\033[0m %s' % (prefix, name))

    # Get an environment by name.
    def __getitem__(self, key):
        for (k, v) in self.envs:
            if k and k == key:
                return v

        return None
