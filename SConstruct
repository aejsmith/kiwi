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

import multiprocessing
import os
import SCons.Errors
import sys

# Release information.
version = {
    'KIWI_VER_RELEASE': 0,
    'KIWI_VER_UPDATE': 1,
    'KIWI_VER_REVISION': 0,
}

# Symlink rather than hardlink in the build tree. This helps VSCode to resolve
# includes to the location in the original source tree rather than opening the
# hardlinked version under the build tree.
SetOption('duplicate', 'soft-hard-copy')

# Change the Decider to content-timestamp to speed up the build a bit.
Decider('content-timestamp')

# Option to set -j option automatically for the VS project, since SCons doesn't
# have this itself.
if ARGUMENTS.get('PARALLEL') == '1':
    SetOption('num_jobs', multiprocessing.cpu_count())

# Add the path to our build utilities to the path.
sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path
from kconfig import ConfigParser
from manager import BuildManager
from package import PackageRepository
from toolchain import ToolchainManager
import util
import vcs

# Set the version string.
version['KIWI_VER_STRING'] = '%d.%d' % (version['KIWI_VER_RELEASE'], version['KIWI_VER_UPDATE'])
if version['KIWI_VER_REVISION']:
    version['KIWI_VER_STRING'] += '.%d' % (version['KIWI_VER_REVISION'])
revision = vcs.revision_id()
if revision:
    version['KIWI_VER_STRING'] += '-%s' % (revision)

# Check if Git submodules are up-to-date.
if ARGUMENTS.get('IGNORE_SUBMODULES') != '1' and not vcs.check_submodules():
    raise SCons.Errors.StopError(
        "Submodules outdated. Please run 'git submodule update --init'.")

# Load the build configuration (if it exists yet).
config = ConfigParser('.config')

# Create the build manager.
manager = BuildManager(config)

Export('config', 'manager', 'version')

# Create the host environment and build host utilities.
host_env = manager.create_host(name = 'host')
SConscript(
    'utilities/SConscript',
    variant_dir = os.path.join('build', 'host'), exports = {'env': host_env})

# Add targets to run the configuration interface.
host_env['ENV']['KERNELVERSION'] = version['KIWI_VER_STRING']
Alias('config', host_env.ConfigMenu('__config', ['Kconfig']))

# If the configuration does not exist, all we can do is configure. Raise an
# error to notify the user that they need to configure if they are not trying
# to do so, and don't run the rest of the build.
if not config.configured() or 'config' in COMMAND_LINE_TARGETS:
    util.require_target('config', "Configuration missing or out of date. Please update using 'config' target.")
    Return()

# Initialise the toolchain manager and add the toolchain build target.
toolchain = ToolchainManager(config)
Alias('toolchain', Command('__toolchain', [], Action(toolchain.update, None)))

# If the toolchain is out of date, only allow it to be built.
if toolchain.check() or 'toolchain' in COMMAND_LINE_TARGETS:
    util.require_target('toolchain', "Toolchain out of date. Update using the 'toolchain' target.")
    Return()

# Now set up the target template environment.
manager.init_target(toolchain)

#######################
# Target system build #
#######################

dist_env = manager['dist']

# First we need to set up packages. Parts of the userspace build depend on
# libraries in packages so we need to have this all set up before we run the
# user SConscripts.

# Exclude package development files (headers etc.) from the image for now.
package_exclude = ['system/devel']

packages = PackageRepository(
    manager          = manager,
    config           = config,
    root_dir         = os.path.join('packages', 'repo'),
    work_dir         = os.path.join('build', 'packages', config['ARCH']),
    manifest_exclude = package_exclude)

# Discover all our packages.
packages.load()

# Ensure all packages are extracted to the working directory and build
# manifests. This only does anything when packages are determined to have
# changed.
packages.extract()

build_dir = os.path.join('build', '%s-%s' % (config['ARCH'], config['BUILD']))
SConscript('source/SConscript', variant_dir = build_dir)

# Add our loose data into the manifest.
#
# All files in there are currently tracked as dependencies. If we end up with a
# large amount of data in here it may become expensive for SCons to do
# dependency tracking on all of it. Stuff should be moved out to packages where
# possible instead of putting it in there.
dist_env['MANIFEST'].add_from_dir_tree('data', env = dist_env, tracked = True)

# Add packages to the manifest.
packages.add_manifests(dist_env['MANIFEST'])

# Build a manifest as our default target which builds the whole system.
system_manifest = dist_env.Manifest(os.path.join(build_dir, 'system.manifest'))
Default(system_manifest)

###############
# Image build #
###############

# Images are built into a separate directory since we build a persistent disk
# image that we preserve user data in. Using a separate directory allows the
# build directory to be easily cleared without users losing their persistent
# image.
images_dir = os.path.join('images', config['ARCH'])

# Target for a filesystem archive file.
fs_archive_path = os.path.join(images_dir, 'fs.tar')
dist_env.FSArchive(fs_archive_path)
dist_env['FS_ARCHIVE'] = File(fs_archive_path)

# Add arch-specific targets to generate images.
qemu_binary = config['QEMU_BINARY_' + config['ARCH'].upper()]
qemu_opts  = config['QEMU_OPTS_' + config['ARCH'].upper()]
if config['ARCH'] == 'amd64':
    iso_image_path = os.path.join(images_dir, 'cdrom.iso')
    iso_image = dist_env.ISOImage(iso_image_path)

    disk_image_path = os.path.join(images_dir, 'disk.img')
    disk_image = dist_env.DiskImage(disk_image_path)

    # Persistent image that we do in-place updates on rather than rebuilding
    # from scratch. This preserves user data while updating the installed
    # system.
    user_image_path = os.path.join(images_dir, 'user.img')
    user_image = dist_env.DiskImage(user_image_path, persistent = True)

    # The QEMU target runs with the separate image parts. This is for two
    # reasons:
    #  1. We can't do in-place updates on the combined image.
    #  2. To reduce build times. The .parts file is a dummy file whose build
    #     function builds the separate image parts files without depending on
    #     them. This means SCons doesn't end up checksumming the whole image
    #     files every time we run.
    user_image_parts = File(user_image_path + '.parts')
    Alias('qemu', dist_env.Command('__qemu', [user_image_parts], Action(
        '%s -drive format=raw,file="%s" %s' % (qemu_binary, user_image_path + '.system', qemu_opts),
        None)))
else:
    boot_archive_path = os.path.join(images_dir, 'boot.tar')
    boot_archive = dist_env.BootArchive(boot_archive_path)
    Alias('boot_archive', boot_archive)

    Alias('qemu', dist_env.Command('__qemu', [dist_env['KBOOT'][0], boot_archive_path], Action(
        qemu_binary + ' -kernel ${SOURCES[0]} -initrd ${SOURCES[1]} ' + qemu_opts,
        None)))

# Helper to run GDB attached to QEMU with the appropriate options for the
# current configuration.
Alias('qgdb', dist_env.Command('__qgdb', [], Action(
    'gdb --eval-command="symbol-file %s" --eval-command="set architecture %s" --eval-command="target remote localhost:1234"' %
        (os.path.join(build_dir, 'kernel', 'kernel-unstripped'), config['GDB_ARCH']),
    None)))

###############
# Final steps #
###############

sysroot_env = manager['sysroot']
sysroot_manifest = sysroot_env['MANIFEST']

# Command to update the toolchain sysroot.
Alias('sysroot',
    sysroot_env.Command('__sysroot', sysroot_manifest.dependencies,
        Action(lambda target, source, env: toolchain.sysroot_action(target, source, env), None)))

# Generation compilation database.
compile_commands = host_env.CompilationDatabase(os.path.join('build', 'compile_commands.json'))
host_env.Default(compile_commands)
host_env.Alias("compile_commands", compile_commands)
