#
# Copyright (C) 2009-2011 Alex Smith
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
from urlparse import urlparse
from time import time

# Base class of a toolchain component definition.
class ToolchainComponent:
    def __init__(self, manager):
        self.manager = manager
        self.srcdir = os.path.join(os.getcwd(), 'utilities', 'toolchain')
        self.dldir = self.manager.parentdir

    # Check if the component requires updating.
    def check(self):
        path = os.path.join(self.manager.destdir, '.%s-%s-installed' % (self.name, self.version))
        if not os.path.exists(path):
            return True

        # Check if any of the patches are newer.
        mtime = os.stat(path).st_mtime
        for p in self.patches:
            if os.stat(os.path.join(self.srcdir, p[0])).st_mtime > mtime:
                return True
        return False

    # Download an unpack all sources for the component.
    def download(self):
        for f in self.source:
            name = urlparse(f).path.split('/')[-1]
            target = os.path.join(self.dldir, name)
            if not os.path.exists(os.path.join(self.dldir, name)):
                self.manager.msg(' Downloading source file: %s' % (name))

                # Download to .part and then rename when
                # complete so we can easily do continuing of
                # downloads.
                self.execute('wget -c -O %s %s' % (target + '.part', f))
                os.rename(target + '.part', target)

            # Unpack if this is a tarball.
            if name[-8:] == '.tar.bz2':
                self.execute('tar -C %s -xjf %s' % (self.manager.builddir, target))
            elif name[-7:] == '.tar.gz':
                self.execute('tar -C %s -xzf %s' % (self.manager.builddir, target))

    # Helper function to execute a command and throw an exception if
    # required status not returned.
    def execute(self, cmd, directory='.', expected=0):
        print "+ %s" % (cmd)
        oldcwd = os.getcwd()
        os.chdir(directory)
        if os.system(cmd) != expected:
            os.chdir(oldcwd)
            raise Exception, 'Command did not return expected value'
        os.chdir(oldcwd)

    # Apply all patches for this component.
    def patch(self):
        for (p, d, s) in self.patches:
            name = os.path.join(self.srcdir, p)
            self.execute('patch -Np%d -i %s' % (s, name), d)

    # Performs all required tasks to update this component.
    def _build(self):
        self.manager.msg("Building toolchain component '%s'" % (self.name))
        self.download()
        self.patch()

        # Measure time taken to build.
        start = time()
        self.build()
        end = time()
        self.manager.totaltime += (end - start)

        # Signal that we've updated this.
        self.changed = True
        f = open(os.path.join(self.manager.destdir, '.%s-%s-installed' % (self.name, self.version)), 'w')
        f.write('')
        f.close()

# Component definition for binutils.
class BinutilsComponent(ToolchainComponent):
    name = 'binutils'
    version = '2.23.1'
    source = [
        'http://ftp.gnu.org/gnu/binutils/binutils-' + version + '.tar.bz2',
    ]
    patches = [
        ('binutils-' + version + '-kiwi.patch', 'binutils-' + version, 1),
    ]

    def build(self):
        os.mkdir('binutils-build')

        # Work out configure options to use.
        confopts  = '--prefix=%s ' % (self.manager.destdir)
        confopts += '--target=%s ' % (self.manager.target)
        confopts += '--disable-werror '
        # gold has bugs which cause the generated kernel image to be huge.
        #confopts += '--enable-gold=default '

        # Build and install it.
        self.execute('../binutils-%s/configure %s' % (self.version, confopts), 'binutils-build')
        self.execute('make -j%d' % (self.manager.makejobs), 'binutils-build')
        self.execute('make install', 'binutils-build')

# Component definition for GCC.
class GCCComponent(ToolchainComponent):
    name = 'gcc'
    version = '4.8.1'
    source = [
        'http://ftp.gnu.org/gnu/gcc/gcc-' + version + '/gcc-' + version + '.tar.bz2',
    ]
    patches = [
        ('gcc-' + version + '-no-fixinc.patch', 'gcc-' + version, 1),
        ('gcc-' + version + '-kiwi.patch', 'gcc-' + version, 1),
        ('gcc-' + version + '-autoconf.patch', 'gcc-' + version, 1),
    ]

    def build(self):
        os.mkdir('gcc-build')

        # Work out configure options to use.
        env = ""
        confopts  = '--prefix=%s ' % (self.manager.destdir)
        confopts += '--target=%s ' % (self.manager.target)
        confopts += '--enable-languages=c,c++ '
        confopts += '--disable-libstdcxx-pch '
        confopts += '--disable-shared '
        if os.uname()[0] == 'Darwin':
            env = "CC=clang CXX=clang++ "
            confopts += '--with-libiconv-prefix=/opt/local --with-gmp=/opt/local --with-mpfr=/opt/local'

        # Build and install it.
        self.execute('%s../gcc-%s/configure %s' % (env, self.version, confopts), 'gcc-build')
        self.execute('make -j%d all-gcc' % (self.manager.makejobs), 'gcc-build')
        self.execute('make -j%d all-target-libgcc all-target-libstdc++-v3' % (self.manager.makejobs), 'gcc-build')
        self.execute('make install-gcc install-target-libgcc install-target-libstdc++-v3', 'gcc-build')

# Class to manage building and updating the toolchain.
class ToolchainManager:
    def __init__(self, config):
        self.config = config
        self.parentdir = config['TOOLCHAIN_DIR']
        self.destdir = os.path.join(config['TOOLCHAIN_DIR'], config['TOOLCHAIN_TARGET'])
        self.builddir = os.path.join(self.destdir, 'build-tmp')
        self.target = config['TOOLCHAIN_TARGET']
        self.makejobs = config['TOOLCHAIN_MAKE_JOBS']
        self.totaltime = 0

        # Keep sorted in dependency order.
        self.components = [
            BinutilsComponent(self),
            GCCComponent(self),
        ]

    # Write a status message.
    def msg(self, msg):
        print '\033[0;32m>>>\033[0;1m %s\033[0m' % (msg)

    # Repairs any links within the toolchain directory.
    def repair(self):
        # Remove existing stuff.
        self.remove('%s/%s/sys-include' % (self.destdir, self.target))
        self.remove('%s/%s/lib' % (self.destdir, self.target))

        # Link into the source tree.
        os.symlink('%s/source/include' % (os.getcwd()), '%s/%s/sys-include' % (self.destdir, self.target))
        os.symlink('%s/build/%s-%s/source/libraries' % (os.getcwd(), self.config['ARCH'], self.config['PLATFORM']),
                   '%s/%s/lib' % (self.destdir, self.target))

    # Remove a file, symbolic link or directory tree.
    def remove(self, path):
        if not os.path.lexists(path):
            return

        # Handle symbolic links first as isfile() and isdir() follow
        # links.
        if os.path.islink(path) or os.path.isfile(path):
            os.remove(path)
        elif os.path.isdir(path):
            shutil.rmtree(path)
        else:
            raise Exception, "Unhandled type during remove (%s)" % (path)

    # Build a component.
    def build(self, c):
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

    # Check if an update is required.
    def check(self):
        for c in self.components:
            if c.check():
                return True

        # Nothing needs to be built, check links and clean up.
        self.remove(self.builddir)
        self.repair()
        return False

    # Rebuilds the toolchain if required.
    def update(self, target, source, env):
        if not self.check():
            return 0

        # Remove existing installation.
        self.remove(self.destdir)

        # Create new destination directory, and set up the include link
        # into the source tree.
        os.makedirs('%s/%s' % (self.destdir, self.target))
        os.symlink('%s/source/include' % (os.getcwd()), '%s/%s/sys-include' % (self.destdir, self.target))

        # Build necessary components.
        try:
            for c in self.components:
                self.build(c)
        except Exception, e:
            self.msg('Exception during toolchain build: \033[0;0m%s' % (str(e)))
            return 1

        # Move the directory containing built libraries and linker
        # scripts to where the build system expects them.
        os.rename('%s/%s/lib' % (self.destdir, self.target), '%s/%s/toolchain-lib' % (self.destdir, self.target))

        # Create library directory link and clean up.
        self.repair()
        self.remove(self.builddir)
        self.msg('Toolchain updated in %d seconds' % (self.totaltime))
        return 0
