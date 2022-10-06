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

# Variables to set in host environments. Don't build C code with our normal
# warning flags, Kconfig and Flex/Bison code won't compile with them. Also
# older host G++ versions don't support some flags.
host_flags = {
    'CCFLAGS': ['-pipe'],
    'CFLAGS': ['-std=gnu99'],
    'CXXFLAGS': [f for f in cc_warning_flags if f not in [
        '-Wmissing-declarations', '-Wno-variadic-macros',
        '-Wno-unused-but-set-variable']],
    'YACCFLAGS': ['-d'],
}

#########################
# Internal build setup. #
#########################

import os, sys, SCons.Errors
import multiprocessing

SetOption('duplicate', 'soft-hard-copy')

# Option to set -j option automatically for the VS project, since SCons doesn't
# have this itself.
if ARGUMENTS.get('PARALLEL') == '1':
    SetOption('num_jobs', multiprocessing.cpu_count())

# Add the path to our build utilities to the path.
sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path
from manager import BuildManager
from kconfig import ConfigParser
from toolchain import ToolchainManager
from util import RequireTarget
import vcs

# Set the version string.
version['KIWI_VER_STRING'] = '%d.%d' % (
    version['KIWI_VER_RELEASE'],
    version['KIWI_VER_UPDATE'])
if version['KIWI_VER_REVISION']:
    version['KIWI_VER_STRING'] += '.%d' % (version['KIWI_VER_REVISION'])
revision = vcs.revision_id()
if revision:
    version['KIWI_VER_STRING'] += '-%s' % (revision)

# Check if Git submodules are up-to-date.
if ARGUMENTS.get('IGNORE_SUBMODULES') != '1' and not vcs.check_submodules():
    raise SCons.Errors.StopError(
        "Submodules outdated. Please run 'git submodule update --init'.")

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')

host_env = Environment(ENV = os.environ, tools = ['default', 'textfile'])
target_env = Environment(platform = 'posix', ENV = os.environ, tools = ['default', 'textfile'])

host_env.Tool('compilation_db', toolpath = ['utilities/build'])
target_env.Tool('compilation_db', toolpath = ['utilities/build'])

manager = BuildManager(host_env, target_env)

# Load the build configuration (if it exists yet).
config = ConfigParser('.config')
manager.AddVariable('_CONFIG', config)

Export('config', 'manager', 'version')

# Set up the host environment template.
for (k, v) in host_flags.items():
    host_env[k] = v

# Darwin hosts probably have needed libraries in /opt.
if os.uname()[0] == 'Darwin':
    host_env['CPPPATH'] = ['/opt/local/include']
    host_env['LIBPATH'] = ['/opt/local/lib']

# Create the host environment and build host utilities.
env = manager.CreateHost(name = 'host')
SConscript('utilities/SConscript', variant_dir = os.path.join('build', 'host'),
    exports = ['env'])

# Add targets to run the configuration interface.
env['ENV']['KERNELVERSION'] = version['KIWI_VER_STRING']
Alias('config', env.ConfigMenu('__config', ['Kconfig']))

# If the configuration does not exist, all we can do is configure. Raise an
# error to notify the user that they need to configure if they are not trying
# to do so, and don't run the rest of the build.
if not config.configured() or 'config' in COMMAND_LINE_TARGETS:
    RequireTarget('config',
        "Configuration missing or out of date. Please update using 'config' target.")
    Return()

# Initialise the toolchain manager and add the toolchain build target.
toolchain = ToolchainManager(config)
Alias('toolchain', Command('__toolchain', [], Action(toolchain.update, None)))

# If the toolchain is out of date, only allow it to be built.
if toolchain.check() or 'toolchain' in COMMAND_LINE_TARGETS:
    RequireTarget('toolchain',
        "Toolchain out of date. Update using the 'toolchain' target.")
    Return()

# Now set up the target template environment.
for (k, v) in target_flags.items():
    target_env[k] = v
for (k, v) in target_type_flags[config['BUILD']].items():
    target_env[k] += v

# Clang's integrated assembler doesn't support 16-bit code.
target_env['ASFLAGS'] = ['-D__ASM__', '-no-integrated-as']

# Set correct shared library link flags.
target_env['SHCCFLAGS']   = '$CCFLAGS -fPIC -DSHARED'
target_env['SHLINKFLAGS'] = '$LINKFLAGS -shared -Wl,-soname,${TARGET.name}'

# Override default assembler - it uses as directly, we want to go through the
# compiler.
target_env['ASCOM'] = '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES'

# Add an action for ASM files in a shared library.
from SCons.Tool import createObjBuilders
static_obj, shared_obj = createObjBuilders(target_env)
shared_obj.add_action('.S', Action('$CC $_CCCOMCOM $ASFLAGS -DSHARED -c -o $TARGET $SOURCES', '$ASCOMSTR'))

# Add in extra compilation flags from the configuration.
if 'ARCH_ASFLAGS' in config:
    target_env['ASFLAGS'] += config['ARCH_ASFLAGS'].split()
if 'ARCH_CCFLAGS' in config:
    target_env['CCFLAGS'] += config['ARCH_CCFLAGS'].split()

target_env['CCFLAGS']  += config['EXTRA_CCFLAGS'].split()
target_env['CFLAGS']   += config['EXTRA_CFLAGS'].split()
target_env['CXXFLAGS'] += config['EXTRA_CXXFLAGS'].split()

# Set paths to toolchain components.
if 'CC' in os.environ and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
    target_env['CC'] = os.environ['CC']
    target_env['ENV']['CCC_CC'] = toolchain.tool_path('clang')

    # Force a rebuild when doing static analysis.
    def decide_if_changed(dependency, target, prev_ni):
        return True
    target_env.Decider(decide_if_changed)
else:
    target_env['CC'] = toolchain.tool_path('clang')
if 'CXX' in os.environ and os.path.basename(os.environ['CXX']) == 'c++-analyzer':
    target_env['CXX'] = os.environ['CXX']
    target_env['ENV']['CCC_CXX'] = toolchain.tool_path('clang++')
else:
    target_env['CXX'] = toolchain.tool_path('clang++')
target_env['AS']      = toolchain.tool_path('as')
target_env['OBJDUMP'] = toolchain.tool_path('objdump')
target_env['READELF'] = toolchain.tool_path('readelf')
target_env['NM']      = toolchain.tool_path('nm')
target_env['STRIP']   = toolchain.tool_path('strip')
target_env['AR']      = toolchain.tool_path('ar')
target_env['RANLIB']  = toolchain.tool_path('ranlib')
target_env['OBJCOPY'] = toolchain.tool_path('objcopy')
target_env['LD']      = toolchain.tool_path('ld')

build_dir = os.path.join('build', '%s-%s' % (config['ARCH'], config['BUILD']))

# Build the target system.
SConscript('source/SConscript', variant_dir = build_dir)

# Now that we have information of all libraries, update the toolchain sysroot.
toolchain.update_sysroot(manager)

# Generation compilation database.
compile_commands = env.CompilationDatabase(os.path.join('build', 'compile_commands.json'))
env.Default(compile_commands)
env.Alias("compiledb", compile_commands)
