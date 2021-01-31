#
# Copyright (C) 2009-2021 Alex Smith
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

import os, sys, shutil
from subprocess import Popen, PIPE
from time import time
from urllib.parse import urlparse

llvm_version = '10.0.1'

def which(program):
    import os
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
        self.manager = manager
        self.destdir = manager.genericdir if self.generic else manager.targetdir

    # Check if the component requires updating.
    def check(self):
        path = os.path.join(self.destdir, '.%s-%s-installed' % (self.name, self.version))
        if not os.path.exists(path):
            return True

        # Check if any of the patches are newer.
        mtime = os.stat(path).st_mtime
        for p in self.patches:
            if os.stat(os.path.join(self.manager.srcdir, p[0])).st_mtime > mtime:
                return True
        return False

    # Download an unpack all sources for the component.
    def download(self):
        for url in self.source:
            name = urlparse(url).path.split('/')[-1]
            target = os.path.join(self.manager.destdir, name)
            if not os.path.exists(target):
                msg(' Downloading source file: %s' % (name))

                # Download to .part and then rename when complete so we can
                # easily do continuing of downloads.
                self.execute('wget -c -O %s %s' % (target + '.part', url))
                os.rename(target + '.part', target)

            # Unpack if this is a tarball.
            if name[-8:] == '.tar.bz2':
                self.execute('tar -C %s -xjf %s' % (self.manager.builddir, target))
            elif name[-7:] == '.tar.gz':
                self.execute('tar -C %s -xzf %s' % (self.manager.builddir, target))
            elif name[-7:] == '.tar.xz':
                self.execute('tar -C %s -xJf %s' % (self.manager.builddir, target))

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
            name = os.path.join(self.manager.srcdir, p)
            self.execute('patch -Np%d -i %s' % (s, name), d)

    # Performs all required tasks to update this component.
    def _build(self):
        msg("Building toolchain component '%s'" % (self.name))
        self.download()

        # Measure time taken to build.
        start = time()
        self.build()
        end = time()
        self.manager.totaltime += (end - start)

        # Signal that we've updated this.
        f = open(os.path.join(self.destdir, '.%s-%s-installed' % (self.name, self.version)), 'w')
        f.write('')
        f.close()

# Component definition for binutils.
class BinutilsComponent(ToolchainComponent):
    name = 'binutils'
    version = '2.36'
    generic = False
    source = [
        'http://ftp.gnu.org/gnu/binutils/binutils-' + version + '.tar.bz2',
    ]
    patches = [
        ('binutils-' + version + '-kiwi.patch', 'binutils-' + version, 1),
    ]

    def build(self):
        self.patch()

        # Work out configure options to use.
        confopts  = '--prefix="%s" ' % (self.destdir)
        confopts += '--target=%s ' % (self.manager.target)
        confopts += '--disable-werror '
        confopts += '--with-sysroot="%s" ' % (os.path.join(self.destdir, 'sysroot'))
        confopts += '--with-lib-path="=/system/lib:=/lib"'

        # gold has bugs which cause the generated kernel image to be huge.
        #confopts += '--enable-gold=default '

        # Build and install it.
        os.mkdir('binutils-build')
        self.execute('../binutils-%s/configure %s' % (self.version, confopts), 'binutils-build')
        self.execute('make -j%d' % (self.manager.makejobs), 'binutils-build')
        self.execute('make install', 'binutils-build')

# Component definition for LLVM/Clang.
class LLVMComponent(ToolchainComponent):
    name = 'llvm'
    version = llvm_version
    generic = True
    source = [
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/llvm-' + version + '.src.tar.xz',
        'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + version + '/clang-' + version + '.src.tar.xz',
    ]
    patches = [
        ('llvm-' + version + '-kiwi.patch', 'llvm-' + version + '.src', 1),
    ]

    def build(self):
        # Move clang sources to the right place.
        os.rename('clang-%s.src' % (self.version), 'llvm-%s.src/tools/clang' % (self.version))

        self.patch()

        # Work out CMake options to use.
        cmakeopts  = '-G "Unix Makefiles" '
        cmakeopts += '-DCMAKE_BUILD_TYPE=Release '
        cmakeopts += '-DLLVM_TARGETS_TO_BUILD="X86;AArch64" '
        cmakeopts += '-DCMAKE_INSTALL_PREFIX="%s" ' % (self.destdir)

        # Build and install it.
        os.mkdir('llvm-build')
        self.execute('cmake %s ../llvm-%s.src' % (cmakeopts, self.version), 'llvm-build')
        self.execute('make -j%d' % (self.manager.makejobs), 'llvm-build')
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
            path = os.path.join(manager.genericdir, 'bin', name)
            wrapper = os.path.join(manager.targetdir, 'bin', '%s-%s' % (manager.target, name))
            f = open(wrapper, 'w')
            f.write('#!/bin/bash\n\n')
            f.write('exec -a "$0" "%s" --sysroot="%s/sysroot" "$@"\n' % (path, manager.targetdir))
            f.close()
            os.chmod(wrapper, 0o755)
        try:
            os.symlink('%s-clang' % (manager.target),
                os.path.join(manager.targetdir, 'bin', '%s-cc' % (manager.target)))
            os.symlink('%s-clang' % (manager.target),
                os.path.join(manager.targetdir, 'bin', '%s-gcc' % (manager.target)))
            os.symlink('%s-clang++' % (manager.target),
                os.path.join(manager.targetdir, 'bin', '%s-c++' % (manager.target)))
            os.symlink('%s-clang++' % (manager.target),
                os.path.join(manager.targetdir, 'bin', '%s-g++' % (manager.target)))
        except:
            pass

# Class to manage building and updating the toolchain.
class ToolchainManager:
    def __init__(self, config):
        self.arch           = config['ARCH']
        self.platform       = config['PLATFORM']
        self.build          = config['BUILD']
        self.destdir        = config['TOOLCHAIN_DIR']
        self.target         = config['TOOLCHAIN_TARGET']
        self.toolchain_arch = config['TOOLCHAIN_ARCH']
        self.makejobs       = config['TOOLCHAIN_MAKE_JOBS']

        self.srcdir     = os.path.join(os.getcwd(), 'utilities', 'toolchain')
        self.genericdir = os.path.join(self.destdir, 'generic')
        self.targetdir  = os.path.join(self.destdir, self.target)
        self.builddir   = os.path.join(self.destdir, 'build-tmp')

        self.totaltime = 0

        self.toolchain = LLVMToolchain(self)

    # Set up the toolchain sysroot.
    def update_sysroot(self, manager):
        sysrootdir = os.path.join(self.targetdir, 'sysroot')
        libdir     = os.path.join(sysrootdir, 'lib')
        includedir = os.path.join(sysrootdir, 'include')
        builddir   = os.path.join(os.getcwd(), 'build', '%s-%s-%s' % (self.arch, self.platform, self.build))

        # Remove any existing sysroot.
        remove(sysrootdir)
        makedirs(sysrootdir)

        # All libraries get placed into a single directory, just link to it.
        os.symlink(os.path.join(builddir, 'lib'), libdir)

        # Now create the include directory. We create symbolic links back to
        # the source tree for the contents of all libraries' header paths.
        makedirs(includedir)
        for (name, lib) in manager.libraries.items():
            for dir in lib['include_paths']:
                def link_tree(targetdir, dir):
                    makedirs(dir)
                    for entry in os.listdir(targetdir):
                        target = os.path.join(targetdir, entry)
                        path = os.path.join(dir, entry)
                        if os.path.isdir(target):
                            link_tree(target, path)
                        else:
                            os.symlink(target, path)
                if type(dir) == tuple:
                    # Link the directory to a specific location.
                    target = os.path.join(os.getcwd(), str(dir[0].srcnode()))
                    path = os.path.join(includedir, dir[1])
                    link_tree(target, path)
                else:
                    # Link everything in the root of the directory into the
                    # root of the sysroot.
                    target = os.path.join(os.getcwd(), str(dir.srcnode()))
                    link_tree(target, includedir)

        # Create a symlink to the compiler-rt builtins library in the build tree.
        runtime_dir = os.path.join(self.genericdir, 'lib', 'clang', llvm_version, 'lib', 'kiwi')
        makedirs(runtime_dir)
        runtime_name = 'libclang_rt.builtins-%s.a' % (self.toolchain_arch)
        runtime_lib = os.path.join(runtime_dir, runtime_name)
        remove(runtime_lib)
        os.symlink(os.path.join(builddir, 'lib', runtime_name), runtime_lib)

    # Build a component.
    def build_component(self, c):
        # Create the target directory and change into it.
        makedirs(self.builddir)
        olddir = os.getcwd()
        os.chdir(self.builddir)

        # Perform the actual build.
        try:
            c._build()
        finally:
            # Change to the old directory and clean up the build directory.
            os.chdir(olddir)
            remove(self.builddir)

    # Get the path to a toolchain utility.
    def tool_path(self, name):
        return os.path.join(self.targetdir, 'bin', self.target + '-' + name)

    # Check if an update is required.
    def check(self):
        for c in self.toolchain.components:
            if c.check():
                return True

        remove(self.builddir)
        return False

    # Rebuilds the toolchain if required.
    def update(self, target, source, env):
        if not self.check():
            msg('Toolchain already up-to-date, nothing to be done')
            return 0

        # Create destination directory.
        makedirs(self.genericdir)
        makedirs(self.targetdir)
        makedirs(os.path.join(self.targetdir, 'bin'))

        self.toolchain.pre_update(self)

        # Build necessary components.
        try:
            for c in self.toolchain.components:
                if c.check():
                    self.build_component(c)
        except Exception as e:
            msg('Exception during toolchain build: \033[0;0m%s' % (str(e)))
            return 1

        remove(self.builddir)
        self.toolchain.post_update(self)

        msg('Toolchain updated in %d seconds' % (self.totaltime))
        return 0
