#
# Copyright (C) 2009-2013 Alex Smith
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

# Release information.
version = {
    'KIWI_VER_RELEASE': 0,
    'KIWI_VER_UPDATE': 1,
    'KIWI_VER_REVISION': 0,
}

# C/C++ warning flags.
cc_warning_flags = [
    '-Wall', '-Wextra', '-Wno-variadic-macros', '-Wno-unused-parameter',
    '-Wwrite-strings', '-Wmissing-declarations', '-Wredundant-decls',
    '-Wno-format', '-Werror', '-Wno-error=unused',
]

# C++ warning flags.
cxx_warning_flags = [
    '-Wold-style-cast', '-Wsign-promo',
]

# Variables to set in host environments. Don't build C code with our normal
# warning flags, Kconfig and Flex/Bison code won't compile with them. Also
# older host G++ versions don't support some flags.
host_flags = {
    'CCFLAGS': ['-pipe'],
    'CFLAGS': ['-std=gnu99'],
    'CXXFLAGS': filter(lambda f: f not in [
        '-Wmissing-declarations', '-Wno-variadic-macros',
        '-Wno-unused-but-set-variable'], cc_warning_flags),
    'YACCFLAGS': ['-d'],
}

# Variables to set in target environments.
target_flags = {
    'CCFLAGS': cc_warning_flags + ['-gdwarf-2', '-pipe'],
    'CFLAGS': ['-std=gnu99'],
    'CXXFLAGS': cxx_warning_flags,

    # Clang's integrated assembler doesn't support 16-bit code.
    'ASFLAGS': ['-D__ASM__', '-no-integrated-as'],

    # Set correct shared library link flags.
    'SHCCFLAGS': '$CCFLAGS -fPIC -DSHARED',
    'SHLINKFLAGS': '$LINKFLAGS -shared -Wl,-soname,${TARGET.name}',

    # Override default assembler - it uses as directly, we want to go
    # through the compiler.
    'ASCOM': '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES',
}

#########################
# Internal build setup. #
#########################

import os, sys, SCons.Errors

# Add the path to our build utilities to the path.
sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path
from kconfig import ConfigParser
from util import RequireTarget
import vcs

# Class for build environment management. Because we have several build
# environments, this class acts like a dictionary of environments, and assists
# in the creation of new ones.
class EnvironmentManager(dict):
    def __init__(self, config):
        dict.__init__(self)
        self.config = config

        # Create compile strings that will be added to all environments.
        verbose = ARGUMENTS.get('V') == '1'
        def compile_string(msg, name):
            if verbose:
                return None
            return ' \033[0;32m%-6s\033[0m %s' % (msg, name)
        self.variables = {
            'ARCOMSTR':     compile_string('AR',     '$TARGET'),
            'ASCOMSTR':     compile_string('ASM',    '$SOURCE'),
            'ASPPCOMSTR':   compile_string('ASM',    '$SOURCE'),
            'CCCOMSTR':     compile_string('CC',     '$SOURCE'),
            'SHCCCOMSTR':   compile_string('CC',     '$SOURCE'),
            'CXXCOMSTR':    compile_string('CXX',    '$SOURCE'),
            'SHCXXCOMSTR':  compile_string('CXX',    '$SOURCE'),
            'YACCCOMSTR':   compile_string('YACC',   '$SOURCE'),
            'LEXCOMSTR':    compile_string('LEX',    '$SOURCE'),
            'LINKCOMSTR':   compile_string('LINK',   '$TARGET'),
            'SHLINKCOMSTR': compile_string('SHLINK', '$TARGET'),
            'RANLIBCOMSTR': compile_string('RANLIB', '$TARGET'),
            'GENCOMSTR':    compile_string('GEN',    '$TARGET'),
            'STRIPCOMSTR':  compile_string('STRIP',  '$TARGET'),
        }

        # Create an array of builders that will be added to all
        # environments.
        self.builders = {
            'LDScript': Builder(action = Action(
                '$CC $_CCCOMCOM $ASFLAGS -E -x c $SOURCE | grep -v "^\#" > $TARGET',
                '$GENCOMSTR')),
        }

    # Merge flags into an environment.
    def merge_flags(self, env, flags):
        if not flags:
            return

        # The MergeFlags function in Environment only handles
        # lists. Add anything else manually.
        merge = {}
        for (k, v) in flags.items():
            if type(v) == list:
                if env.has_key(k):
                    merge[k] = v
                else:
                    env[k] = v
            elif type(v) == dict and env.has_key(k) and type(env[k]) == dict:
                env[k].update(v)
            else:
                env[k] = v
        env.MergeFlags(merge)

    # Perform common setup for an environment.
    def setup_env(self, env, flags):
        # Add variables/builders.
        for (k, v) in self.variables.items():
            env[k] = v
        for (k, v) in self.builders.items():
            env['BUILDERS'][k] = v
        for (k, v) in flags.items():
            env[k] = v

    # Add a variable to all environments and all future environments.
    def AddVariable(self, name, value):
        self.variables[name] = value
        for (k, v) in self.items():
            self[k][name] = value

    # Add a builder to all environments and all future environments.
    def AddBuilder(self, name, builder):
        self.builders[name] = builder
        for (k, v) in self.items():
            self[k]['BUILDERS'][name] = builder

    # Add a build tool to all environments and all future environments.
    def AddTool(self, name, depends, act):
        if type(depends) != list:
            depends = [depends]
        def dep_emitter(target, source, env):
            for dep in depends:
                Depends(target, dep)
            return (target, source)
        self.AddBuilder(name, Builder(action = act, emitter = dep_emitter))

    # Create an environment for building for the host system.
    def CreateHost(self, name, flags = None):
        env = Environment(ENV = os.environ)
        self.setup_env(env, host_flags)
        self.merge_flags(env, flags)

        if os.uname()[0] == 'Darwin':
            env['CPPPATH'] = ['/opt/local/include']
            env['LIBPATH'] = ['/opt/local/lib']

        self[name] = env
        return env

    # Create an environment for building for the target system. This
    # requires that the configuration has been set up correctly.
    def Create(self, name, flags = None):
        assert self.config.configured()

        env = Environment(platform = 'posix', ENV = os.environ)
        self.setup_env(env, target_flags)
        self.merge_flags(env, flags)
        env['CONFIG'] = self.config

        # Add in extra compilation flags from the configuration.
        if self.config.has_key('ARCH_CCFLAGS'):
            env['CCFLAGS'] += self.config['ARCH_CCFLAGS'].split()
        if self.config.has_key('PLATFORM_CCFLAGS'):
            env['CCFLAGS'] += self.config['PLATFORM_CCFLAGS'].split()
        env['CCFLAGS'] += self.config['EXTRA_CCFLAGS'].split()
        env['CFLAGS'] += self.config['EXTRA_CFLAGS'].split()
        env['CXXFLAGS'] += self.config['EXTRA_CXXFLAGS'].split()

        # Set paths to toolchain components.
        def tool_path(name):
            return os.path.join(
                self.config['TOOLCHAIN_DIR'],
                self.config['TOOLCHAIN_TARGET'], 'bin',
                self.config['TOOLCHAIN_TARGET'] + "-" + name)
        if os.environ.has_key('CC') and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
            env['CC'] = os.environ['CC']
            env['ENV']['CCC_CC'] = tool_path('clang')

            # Force a rebuild when doing static analysis.
            def decide_if_changed(dependency, target, prev_ni):
                return True
            env.Decider(decide_if_changed)
        else:
            env['CC'] = tool_path('clang')
        if os.environ.has_key('CXX') and os.path.basename(os.environ['CXX']) == 'c++-analyzer':
            env['CXX'] = os.environ['CXX']
            env['ENV']['CCC_CXX'] = tool_path('clang++')
        else:
            env['CXX'] = tool_path('clang++')
        env['AS']      = tool_path('as')
        env['OBJDUMP'] = tool_path('objdump')
        env['READELF'] = tool_path('readelf')
        env['NM']      = tool_path('nm')
        env['STRIP']   = tool_path('strip')
        env['AR']      = tool_path('ar')
        env['RANLIB']  = tool_path('ranlib')
        env['OBJCOPY'] = tool_path('objcopy')
        env['LD']      = tool_path('ld')

        self.merge_flags(env, flags)
        self[name] = env
        return env

    # Create a new environment based on an existing environment.
    def Clone(self, name, base, flags = None):
        self[name] = self[base].Clone()
        self.merge_flags(self[name], flags)
        return self[name]

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')

# Check if Git submodules are up-to-date.
if ARGUMENTS.get('IGNORE_SUBMODULES') != '1' and not vcs.check_submodules():
    raise SCons.Errors.StopError(
        "Submodules outdated. Please run 'git submodule update --init'.")

# Set the version string.
version['KIWI_VER_STRING'] = '%d.%d' % (
    version['KIWI_VER_RELEASE'],
    version['KIWI_VER_UPDATE'])
if version['KIWI_VER_REVISION']:
    version['KIWI_VER_STRING'] += '.%d' % (version['KIWI_VER_REVISION'])
revision = vcs.revision_id()
if revision:
    version['KIWI_VER_STRING'] += '-%s' % (revision)

# Create the configuration parser and environment manager.
config = ConfigParser('.config')
manager = EnvironmentManager(config)

Export('config', 'manager', 'version')

# Create the host environment and get targets for build utilities.
env = manager.CreateHost('host')
SConscript('utilities/SConscript', variant_dir = os.path.join('build', 'host'),
    exports = ['env'])

# Add targets to run the configuration interface.
env['ENV']['KERNELVERSION'] = version['KIWI_VER_STRING']
Alias('config', env.ConfigMenu('__config', ['Kconfig']))

# Only do the rest of the build if the configuration exists.
if config.configured() and not 'config' in COMMAND_LINE_TARGETS:
    # Initialise the toolchain manager and add the toolchain build target.
    from toolchain import ToolchainManager
    toolchain = ToolchainManager(config)
    Alias('toolchain', Command('__toolchain', [], Action(toolchain.update, None)))

    # If the toolchain is out of date, only allow it to be built.
    if toolchain.check() != 0:
        RequireTarget('toolchain',
            "Toolchain out of date. Update using the 'toolchain' target.")
    else:
        SConscript('source/SConscript', variant_dir = os.path.join('build',
            '%s-%s' % (config['ARCH'], config['PLATFORM'])))
else:
    # Configuration does not exist. All we can do is configure.
    RequireTarget('config',
        "Configuration missing or out of date. Please update using 'config' target.")
