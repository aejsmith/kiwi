#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

import os
import shutil
import time
from urllib.parse import urlparse

llvm_major_version = '16'
llvm_version = '16.0.6'

def which(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None

def msg(msg):
    print('\033[0;32m>>>\033[0;1m %s\033[0m' % (msg))

def remove(path):
    if not os.path.lexists(path):
        return

    # Handle symbolic links first as isfile() and isdir() follow links.
    if os.path.islink(path) or os.path.isfile(path):
        os.remove(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)
    else:
        raise Exception('Unhandled type during remove (%s)' % (path))

def makedirs(path):
    try:
        os.makedirs(path)
    except:
        pass

# Base class of a toolchain component definition.
class ToolchainComponent:
    def __init__(self, manager):
        self.manager  = manager
        self.dest_dir = manager.generic_dir if self.generic else manager.target_dir

    # Check if the component requires updating.
    def check(self):
        path = os.path.join(self.dest_dir, '.%s-%s-installed' % (self.name, self.version))
        if not os.path.exists(path):
            return True

        # Check if any of the patches are newer.
        mtime = os.stat(path).st_mtime
        for p in self.patches:
            if os.stat(os.path.join(self.manager.src_dir, p[0])).st_mtime > mtime:
                return True
        return False

    # Download an unpack all sources for the component.
    def download(self):
        for url in self.source:
            name = urlparse(url).path.split('/')[-1]
            target = os.path.join(self.manager.dest_dir, name)
            if not os.path.exists(target):
                msg(' Downloading source file: %s' % (name))

                # Download to .part and then rename when complete so we can
                # easily do continuing of downloads.
                self.execute('wget -c -O %s %s' % (target + '.part', url))
                os.rename(target + '.part', target)

            # Unpack if this is a tarball.
            if name[-8:] == '.tar.bz2':
                self.execute('tar -C %s -xjf %s' % (self.manager.build_dir, target))
            elif name[-7:] == '.tar.gz':
                self.execute('tar -C %s -xzf %s' % (self.manager.build_dir, target))
            elif name[-7:] == '.tar.xz':
                self.execute('tar -C %s -xJf %s' % (self.manager.build_dir, target))

    # Helper function to execute a command and throw an exception if required
    # status not returned.
    def execute(self, cmd, directory = '.', expected = 0):
        print("+ %s" % (cmd))
        oldcwd = os.getcwd()
        os.chdir(directory)
        if os.system(cmd) != expected:
            os.chdir(oldcwd)
            raise Exception('Command did not return expected value')
        os.chdir(oldcwd)

    # Apply all patches for this component.
    def patch(self):
        for (p, d, s) in self.patches:
            name = os.path.join(self.manager.src_dir, p)
            self.execute('patch -Np%d -i %s' % (s, name), d)

    # Performs all required tasks to update this component.
    def _build(self):
        msg("Building toolchain component '%s'" % (self.name))
        self.download()

        # Measure time taken to build.
        start = time.time()
        self.build()
        end = time.time()
        self.manager.totaltime += (end - start)

        # Signal that we've updated this.
        f = open(os.path.join(self.dest_dir, '.%s-%s-installed' % (self.name, self.version)), 'w')
        f.write('')
        f.close()

# Component definition for binutils.
class BinutilsComponent(ToolchainComponent):
    name = 'binutils'
    version = '2.40'
    generic = False
    source = [
        'http://ftp.gnu.org/gnu/binutils/binutils-' + version + '.tar.xz',
    ]
    patches = [
        ('binutils-' + version + '-kiwi.patch', 'binutils-' + version, 1),
    ]

    def build(self):
        self.patch()

        # Work out configure options to use.
        conf_opts  = '--prefix="%s" ' % (self.dest_dir)
        conf_opts += '--target=%s ' % (self.manager.target)
        conf_opts += '--disable-werror '
        conf_opts += '--with-sysroot="%s" ' % (os.path.join(self.dest_dir, 'sysroot'))
        conf_opts += '--with-lib-path="=/system/lib:=/lib"'

        # gold has bugs which cause the generated kernel image to be huge.
        #conf_opts += '--enable-gold=default '

        # Build and install it.
        os.mkdir('binutils-build')
        self.execute('../binutils-%s/configure %s' % (self.version, conf_opts), 'binutils-build')
        self.execute('make -j%d' % (self.manager.make_jobs), 'binutils-build')
        self.execute('make install', 'binutils-build')

# Component definition for LLVM/Clang.
class LLVMComponent(ToolchainComponent):
    name = 'llvm'
    version = llvm_version
    generic = True
    source = [
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/llvm-' + version + '.src.tar.xz',
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/clang-' + version + '.src.tar.xz',
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/cmake-' + version + '.src.tar.xz',
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/third-party-' + version + '.src.tar.xz',
    ]
    patches = [
        ('llvm-' + version + '-kiwi.patch', 'llvm-' + version + '.src', 1),
    ]

    def build(self):
        os.rename('clang-%s.src' % (self.version), 'llvm-%s.src/tools/clang' % (self.version))
        os.rename('cmake-%s.src' % (self.version), 'cmake')
        os.rename('third-party-%s.src' % (self.version), 'third-party')

        self.patch()

        # Work out CMake options to use.
        cmake_opts  = '-G "Unix Makefiles" '
        cmake_opts += '-DCMAKE_BUILD_TYPE=Release '
        cmake_opts += '-DLLVM_TARGETS_TO_BUILD="X86;AArch64" '
        cmake_opts += '-DLLVM_INCLUDE_BENCHMARKS=OFF '
        cmake_opts += '-DCMAKE_INSTALL_PREFIX="%s" ' % (self.dest_dir)

        # Build and install it.
        os.mkdir('llvm-build')
        self.execute('cmake %s ../llvm-%s.src' % (cmake_opts, self.version), 'llvm-build')
        self.execute('make -j%d' % (self.manager.make_jobs), 'llvm-build')
        self.execute('make install', 'llvm-build')

# Base class for a toolchain.
class Toolchain:
    def pre_update(self, manager):
        pass

    def post_update(self, manager):
        pass

# LLVM-based toolchain.
class LLVMToolchain(Toolchain):
    def __init__(self, manager):
        self.components = [
            BinutilsComponent(manager),
            LLVMComponent(manager),
        ]

    def pre_update(self, manager):
        # Create clang wrapper scripts. The wrapper script is needed to pass
        # the correct sysroot path for the target. The exec sets the executable
        # name for clang to the wrapper script path - this allows clang to
        # determine the target and the tool directory properly.
        for name in ['clang', 'clang++']:
            path = os.path.join(manager.generic_dir, 'bin', name)
            wrapper = os.path.join(manager.target_dir, 'bin', '%s-%s' % (manager.target, name))
            f = open(wrapper, 'w')
            f.write('#!/bin/bash\n\n')
            f.write('exec -a "$0" "%s" --sysroot="%s/sysroot" "$@"\n' % (path, manager.target_dir))
            f.close()
            os.chmod(wrapper, 0o755)
        try:
            os.symlink('%s-clang' % (manager.target),
                os.path.join(manager.target_dir, 'bin', '%s-cc' % (manager.target)))
            os.symlink('%s-clang' % (manager.target),
                os.path.join(manager.target_dir, 'bin', '%s-gcc' % (manager.target)))
            os.symlink('%s-clang++' % (manager.target),
                os.path.join(manager.target_dir, 'bin', '%s-c++' % (manager.target)))
            os.symlink('%s-clang++' % (manager.target),
                os.path.join(manager.target_dir, 'bin', '%s-g++' % (manager.target)))
        except:
            pass

# Class to manage building and updating the toolchain.
class ToolchainManager:
    def __init__(self, config):
        self.arch           = config['ARCH']
        self.build          = config['BUILD']
        self.dest_dir       = config['TOOLCHAIN_DIR']
        self.target         = config['TOOLCHAIN_TARGET']
        self.toolchain_arch = config['TOOLCHAIN_ARCH']
        self.make_jobs      = config['TOOLCHAIN_MAKE_JOBS']

        self.src_dir     = os.path.join(os.getcwd(), 'utilities', 'toolchain')
        self.generic_dir = os.path.join(self.dest_dir, 'generic')
        self.target_dir  = os.path.join(self.dest_dir, self.target)
        self.build_dir   = os.path.join(self.dest_dir, 'build-tmp')
        self.sysroot_dir = os.path.join(self.target_dir, 'sysroot')

        self.totaltime = 0

        self.toolchain = LLVMToolchain(self)

    # Set up toolchain links that must always be up to date.
    def setup_required_links(self):
        build_dir = os.path.join(os.getcwd(), 'build', '%s-%s' % (self.arch, self.build))

        # Create a symlink to the compiler-rt builtins library in the build tree.
        runtime_dir = os.path.join(self.generic_dir, 'lib', 'clang', llvm_major_version, 'lib', 'kiwi')
        makedirs(runtime_dir)
        runtime_name = 'libclang_rt.builtins-%s.a' % (self.toolchain_arch)
        runtime_lib = os.path.join(runtime_dir, runtime_name)
        remove(runtime_lib)
        os.symlink(os.path.join(build_dir, 'lib', runtime_name), runtime_lib)

    # Set up the toolchain sysroot.
    def sysroot_action(self, target, source, env):
        manifest = env['MANIFEST']
        manifest.finalise()

        # Remove any existing sysroot.
        remove(self.sysroot_dir)
        makedirs(self.sysroot_dir)

        # Add contents. We symlink back to the target files so we don't have to
        # rebuild just to change contents of the file.
        actions = manifest.get_actions()
        for path in actions.dirs:
            makedirs(os.path.join(self.sysroot_dir, path))
        for (path, target) in actions.links:
            os.symlink(target, os.path.join(self.sysroot_dir, path))
        for (path, target) in actions.files:
            target = os.path.join(os.getcwd(), str(target))
            os.symlink(target, os.path.join(self.sysroot_dir, path))

    # Build a component.
    def build_component(self, c):
        # Create the target directory and change into it.
        makedirs(self.build_dir)
        olddir = os.getcwd()
        os.chdir(self.build_dir)

        # Perform the actual build.
        try:
            c._build()
        finally:
            # Change to the old directory and clean up the build directory.
            os.chdir(olddir)
            remove(self.build_dir)

    # Get the path to a generic toolchain utility.
    def generic_tool_path(self, name):
        return os.path.join(self.generic_dir, 'bin', name)

    # Get the path to a toolchain utility.
    def tool_path(self, name):
        return os.path.join(self.target_dir, 'bin', self.target + '-' + name)

    # Check if an update is required.
    def check(self):
        for c in self.toolchain.components:
            if c.check():
                return True

        remove(self.build_dir)
        return False

    # Rebuilds the toolchain if required.
    def update(self, target, source, env):
        if not self.check():
            msg('Toolchain already up-to-date, nothing to be done')
            return 0

        # Create destination directory.
        makedirs(self.generic_dir)
        makedirs(self.target_dir)
        makedirs(os.path.join(self.target_dir, 'bin'))

        self.toolchain.pre_update(self)

        # Build necessary components.
        try:
            for c in self.toolchain.components:
                if c.check():
                    self.build_component(c)
        except Exception as e:
            msg('Exception during toolchain build: \033[0;0m%s' % (str(e)))
            return 1

        remove(self.build_dir)
        self.toolchain.post_update(self)

        msg('Toolchain updated in %d seconds' % (self.totaltime))
        return 0
