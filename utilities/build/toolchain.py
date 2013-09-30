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

import os, sys, shutil
from subprocess import Popen, PIPE
from time import time
from urlparse import urlparse

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
            target = os.path.join(self.manager.dldir, name)
            if not os.path.exists(target):
                self.manager.msg(' Downloading source file: %s' % (name))

                # Download to .part and then rename when complete so we can
                # easily do continuing of downloads.
                self.execute('wget -c -O %s %s' % (target + '.part', url))
                os.rename(target + '.part', target)

            # Unpack if this is a tarball.
            if name[-8:] == '.tar.bz2':
                self.execute('tar -C %s -xjf %s' % (self.manager.builddir, target))
            elif name[-7:] == '.tar.gz':
                self.execute('tar -C %s -xzf %s' % (self.manager.builddir, target))

    # Helper function to execute a command and throw an exception if required
    # status not returned.
    def execute(self, cmd, directory = '.', expected = 0):
        print "+ %s" % (cmd)
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
        self.manager.msg("Building toolchain component '%s'" % (self.name))
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
    version = '2.23.1'
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
        confopts  = '--prefix=%s ' % (self.destdir)
        confopts += '--target=%s ' % (self.manager.target)
        confopts += '--disable-werror '
        confopts += '--with-sysroot=%s ' % (os.path.join(self.destdir, 'sysroot'))

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
    version = '3.3'
    generic = True
    source = [
        'http://llvm.org/releases/' + version + '/llvm-' + version + '.src.tar.gz',
        'http://llvm.org/releases/' + version + '/cfe-' + version + '.src.tar.gz',
    ]
    patches = [
        ('llvm-' + version + '-kiwi.patch', 'llvm-' + version + '.src', 1),
    ]

    def build(self):
        # Move clang sources to the right place.
        os.rename('cfe-%s.src' % (self.version), 'llvm-%s.src/tools/clang' % (self.version))

        self.patch()

        # Work out configure options to use.
        confopts  = '--prefix=%s ' % (self.destdir)
        confopts += '--enable-optimized '
        confopts += '--enable-targets=x86,x86_64,arm '

        # LLVM needs Python 2 to build.
        pythons = ['python2', 'python2.7', 'python']
        for python in pythons:
            path = which(python)
            if path:
                confopts += '--with-python=%s' % (path)
                break

        # Build and install it.
        os.mkdir('llvm-build')
        self.execute('../llvm-%s.src/configure %s' % (self.version, confopts), 'llvm-build')
        self.execute('make -j%d' % (self.manager.makejobs), 'llvm-build')
        self.execute('make install', 'llvm-build')

# Component definition for Compiler-RT.
class CompilerRTComponent(ToolchainComponent):
    name = 'compiler-rt'
    version = '3.3'
    generic = False
    source = [
        'http://llvm.org/releases/' + version + '/compiler-rt-' + version + '.src.tar.gz',
    ]
    patches = [
        ('compiler-rt-' + version + '-kiwi.patch', 'compiler-rt-' + version + '.src', 1),
    ]

    def build(self):
        self.patch()

        # Build it.
        self.execute('make CC=%s AR=%s RANLIB=%s clang_kiwi' % (
            self.manager.tool_path('clang'),
            self.manager.tool_path('ar'),
            self.manager.tool_path('ranlib')), 'compiler-rt-%s.src' % self.version)

        # Install it. Iterate directories because some targets build multiple
        # libraries (e.g. i386 and x86_64).
        archs = os.listdir(os.path.join('compiler-rt-%s.src' % self.version, 'clang_kiwi'))
        for arch in archs:
            shutil.copyfile(os.path.join('compiler-rt-%s.src' % self.version, 'clang_kiwi',
                    arch, 'libcompiler_rt.a'),
                os.path.join(self.manager.genericdir, 'lib', 'clang', self.version,
                    'libclang_rt.%s.a' % arch))

# Class to manage building and updating the toolchain.
class ToolchainManager:
    def __init__(self, config):
        self.config = config
        self.srcdir = os.path.join(os.getcwd(), 'utilities', 'toolchain')
        self.destdir = config['TOOLCHAIN_DIR']
        self.target = config['TOOLCHAIN_TARGET']
        self.genericdir = os.path.join(self.destdir, 'generic')
        self.targetdir = os.path.join(self.destdir, self.target)
        self.builddir = os.path.join(self.destdir, 'build-tmp')
        self.dldir = self.destdir
        self.makejobs = config['TOOLCHAIN_MAKE_JOBS']
        self.totaltime = 0

        # Keep sorted in dependency order.
        self.components = [
            BinutilsComponent(self),
            LLVMComponent(self),
            CompilerRTComponent(self),
        ]

    # Write a status message.
    def msg(self, msg):
        print '\033[0;32m>>>\033[0;1m %s\033[0m' % (msg)

    # Remove a file, symbolic link or directory tree.
    def remove(self, path):
        if not os.path.lexists(path):
            return

        # Handle symbolic links first as isfile() and isdir() follow links.
        if os.path.islink(path) or os.path.isfile(path):
            os.remove(path)
        elif os.path.isdir(path):
            shutil.rmtree(path)
        else:
            raise Exception('Unhandled type during remove (%s)' % (path))

    # Make a directory and all intermediate directories, ignoring error if it
    # already exists.
    def makedirs(self, path):
        try:
            os.makedirs(path)
        except:
            pass

    # Create the clang wrappers.
    def create_wrappers(self):
        # Create clang wrapper scripts. The wrapper script is needed to pass
        # the correct sysroot path for the target. The exec sets the executable
        # name for clang to the wrapper script path - this allows clang to
        # determine the target and the tool directory properly.
        for name in ['clang', 'clang++']:
            path = os.path.join(self.genericdir, 'bin', name)
            wrapper = os.path.join(self.targetdir, 'bin', '%s-%s' % (self.target, name))
            f = open(wrapper, 'w')
            f.write('#!/bin/sh\n\n')
            f.write('exec -a $0 %s --sysroot=%s/sysroot $*\n' % (path, self.targetdir))
            f.close()
            os.chmod(wrapper, 0755)
        try:
            os.symlink('%s-clang' % (self.target),
                os.path.join(self.targetdir, 'bin', '%s-cc' % (self.target)))
            os.symlink('%s-clang' % (self.target),
                os.path.join(self.targetdir, 'bin', '%s-gcc' % (self.target)))
            os.symlink('%s-clang++' % (self.target),
                os.path.join(self.targetdir, 'bin', '%s-c++' % (self.target)))
            os.symlink('%s-clang++' % (self.target),
                os.path.join(self.targetdir, 'bin', '%s-g++' % (self.target)))
        except:
            pass

    # Set up the toolchain sysroot.
    def update_sysroot(self, manager):
        sysrootdir = os.path.join(self.targetdir, 'sysroot')
        libdir = os.path.join(sysrootdir, 'lib')
        includedir = os.path.join(sysrootdir, 'include')
        builddir = os.path.join(os.getcwd(), 'build',
            '%s-%s' % (self.config['ARCH'], self.config['PLATFORM']))

        # Remove any existing sysroot.
        self.remove(sysrootdir)
        self.makedirs(sysrootdir)

        # All libraries get placed into a single directory, just link to it.
        os.symlink(os.path.join(builddir, 'lib'), libdir)

        # Now create the include directory. We create symbolic links back to
        # the source tree for the contents of all libraries' header paths.
        self.makedirs(os.path.join(self.targetdir, 'sysroot', 'include'))
        for (name, lib) in manager.libraries.items():
            for dir in lib['include_paths']:
                def link_tree(targetdir, dir):
                    self.makedirs(dir)
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

    # Build a component.
    def build_component(self, c):
        # Create the target directory and change into it.
        os.makedirs(self.builddir)
        olddir = os.getcwd()
        os.chdir(self.builddir)

        # Perform the actual build.
        try:
            c._build()
        finally:
            # Change to the old directory and clean up the build directory.
            os.chdir(olddir)
            self.remove(self.builddir)

    # Get the path to a toolchain utility.
    def tool_path(self, name):
        return os.path.join(self.targetdir, 'bin', self.target + '-' + name)

    # Check if an update is required.
    def check(self):
        for c in self.components:
            if c.check():
                return True

        self.remove(self.builddir)
        return False

    # Rebuilds the toolchain if required.
    def update(self, target, source, env):
        if not self.check():
            self.msg('Toolchain already up-to-date, nothing to be done')
            return 0

        # Remove existing installation. Only remove generic/target directories
        # when a generic/target component requires updating, this avoids
        # rebuilding LLVM when we only need to rebuild Binutils and vice-versa.
        for c in self.components:
            if c.check():
                self.remove(self.genericdir if c.generic else self.targetdir)

        # Create new destination directory.
        self.makedirs(self.genericdir)
        self.makedirs(self.targetdir)
        self.makedirs(os.path.join(self.targetdir, 'bin'))

        # Need to do this first as compiler-rt build requires wrapper in place.
        self.create_wrappers()

        # Build necessary components.
        try:
            for c in self.components:
                if c.check():
                    self.build_component(c)
        except Exception, e:
            self.msg('Exception during toolchain build: \033[0;0m%s' % (str(e)))
            return 1

        self.remove(self.builddir)
        self.msg('Toolchain updated in %d seconds' % (self.totaltime))
        return 0
