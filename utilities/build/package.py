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

from distutils.command.build import build
from manifest import *
from SCons.Script import *
import glob
import hashlib
import json
import os
import shutil

# Default locations to add for libraries.
DEFAULT_INCLUDE_PATH = 'system/devel/include'
DEFAULT_LIB_PATH = 'system/lib'

class Package:
    # Path and checksum of the .package file.
    path     = ''
    checksum = ''

    # Deserialised properties.
    name      = ''
    version   = ''
    revision  = ''
    archs     = []
    libraries = {}

    # Full name: <name>-<version>-<revision>
    full_name = ''

    # Manifest for the package contents (all untracked files).
    manifest = None

    def __init__(self, path):
        self.path = path

        result = {}
        with open(self.path, 'r') as package_file:
            contents = package_file.read()
            self.checksum = hashlib.md5(contents.encode('utf-8')).hexdigest()
            result = json.loads(contents)

        self.name      = result.get('name', None)
        self.version   = result.get('version', None)
        self.revision  = result.get('revision', None)
        self.archs     = result.get('archs', [])
        self.libraries = result.get('libraries', {})

        is_valid = True
        is_valid = is_valid and (self.name and isinstance(self.name, str))
        is_valid = is_valid and (self.version and isinstance(self.version, str))
        is_valid = is_valid and (self.revision and isinstance(self.revision, str))
        is_valid = is_valid and (isinstance(self.archs, list))
        is_valid = is_valid and (isinstance(self.libraries, dict))

        if is_valid:
            for (name, lib) in self.libraries.items():
                is_valid = is_valid and (isinstance(lib, dict))
                is_valid = is_valid and (isinstance(lib.get('build_libraries', []), list))
                is_valid = is_valid and (isinstance(lib.get('include_paths', []), list))

        if not is_valid:
            raise Exception('Invalid package metadata')

        self.full_name = '%s-%s-%s' % (self.name, self.version, self.revision)

    # Check if this package file matches another, used to check if extracted
    # packages are up to date. We just use the checksum of the package file
    # itself. If the binary changes, you need to bump the revision to cause the
    # package to be updated.
    def compare(self, other_path):
        try:
            with open(other_path, 'r') as other_file:
                contents = other_file.read()
                checksum = hashlib.md5(contents.encode('utf-8')).hexdigest()
            return checksum == self.checksum
        except:
            return False

class PackageRepository:
    # manager          = BuildManager instance.
    # config           = ConfigParser instance.
    # root_dir         = Package repository root directory.
    # work_dir         = Directory to extract packages to.
    # manifest_exclude = Paths to exclude from the manifest. If the path matches
    #                    a directory, anything within that directory will be
    #                    excluded. Paths specified will still be present in the
    #                    work directory, but not be copied to images.
    def __init__(self, manager, config, root_dir, work_dir, manifest_exclude = []):
        self.manager          = manager
        self.config           = config
        self.root_dir         = root_dir
        self.work_dir         = work_dir
        self.bin_dir          = os.path.join(root_dir, config['ARCH'])
        self.manifest_exclude = manifest_exclude

        self.packages = {}

    # Load all package definitions from the repository.
    def load(self):
        package_paths = glob.glob(os.path.join(self.root_dir, '*.package'))

        for package_path in package_paths:
            try:
                package = Package(package_path)
            except Exception as e:
                print("Failed to load package '%s': %s" % (package_path, repr(e)))
                continue

            # Ignore packages not supported for our architecture.
            if not self.config['ARCH'] in package.archs:
                continue

            # Ensure that the binary file exists, skip it with a warning if not.
            bin_path = self._get_bin_path(package)
            if not os.path.exists(bin_path):
                print("Package binary '%s' does not exist" % (bin_path))
                continue

            self.packages[package.name] = package

    def _extract_package(self, package):
        # Directory we extract the package contents to.
        work_package_dir = os.path.join(self.work_dir, package.name)

        # Manifest file we'll output for the package.
        work_manifest_path = os.path.join(self.work_dir, '%s.manifest' % (package.name))

        # When we extract a package, we copy the .package file into the work
        # directory. We compare the source package file against this to
        # determine if we need to update the package.
        work_package_path = os.path.join(self.work_dir, '%s.package' % (package.name))
        if package.compare(work_package_path):
            # Load its existing manifest, setting targets relative to the
            # extracted directory.
            try:
                package.manifest = Manifest()
                package.manifest.deserialise(work_manifest_path, work_package_dir)
                return
            except:
                # We can't load it, we need to re-extract it.
                pass

        self.manager.compile_log('extract_package', 'PACKAGE', work_package_path)

        # Clear out existing package directory and create it.
        try:
            shutil.rmtree(work_package_dir)
        except FileNotFoundError:
            pass
        os.makedirs(work_package_dir)

        # Extract the package.
        bin_path = self._get_bin_path(package)
        ret = os.system('tar -C "%s" -xJf "%s"' % (work_package_dir, bin_path))
        if ret != 0:
            print("Failed to extract package '%s': %d" % (bin_path, ret))

            # We can't use this.
            self.packages.pop(package.name)
            return

        # Build a manifest for the package.
        package.manifest = Manifest()

        # Add contents untracked, so that we do not incur SCons overhead for
        # tracking/checksumming the files. Our own tracking is used to decide
        # when the files are up to date. Users could modify the package
        # contents in the work directory, but this is not supported.
        package.manifest.add_from_dir_tree(work_package_dir, tracked = False)

        # Remove exclusions. Need to copy list as we will modify the dictionary.
        entry_paths = list(package.manifest.entries.keys())
        for entry_path in entry_paths:
            for exclude_path in self.manifest_exclude:
                if entry_path == exclude_path or entry_path.startswith(exclude_path + '/'):
                    package.manifest.entries.pop(entry_path)

        # Checksum and serialise.
        package.manifest.finalise()
        package.manifest.serialise(work_manifest_path)

        # Copy the package file now that we've extracted it.
        shutil.copy(package.path, work_package_path)

    # Extract all packages into the working directory and build manifests. This
    # only updates packages that are determined to have changed.
    def extract(self):
        try:
            os.makedirs(self.work_dir)
        except FileExistsError:
            pass

        for package in self.packages.values():
            self._extract_package(package)

    # Add library records to the build manager and sysroot.
    def add_libraries(self):
        env     = self.manager.target_template
        sysroot = self.manager['sysroot']['MANIFEST']

        for package in self.packages.values():
            work_package_dir = os.path.join(self.work_dir, package.name)

            for (name, lib) in package.libraries.items():
                build_libraries = lib.get('build_libraries', [])
                include_paths   = lib.get('include_paths', [])
                lib_paths       = lib.get('lib_paths', [])

                # Use default locations if not specified.
                if not include_paths:
                    include_paths = [DEFAULT_INCLUDE_PATH]
                if not lib_paths:
                    lib_paths = [DEFAULT_LIB_PATH]

                # Map paths into the extracted package.
                include_paths = [os.path.join(work_package_dir, p) for p in include_paths]
                lib_paths     = [os.path.join(work_package_dir, p) for p in lib_paths]

                # Add library record.
                self.manager.add_library(
                    name            = name,
                    build_libraries = build_libraries,
                    include_paths   = [env.Dir(p) for p in include_paths],
                    lib_paths       = [env.Dir(p) for p in lib_paths])

                # Add to the sysroot.
                for path in include_paths:
                    sysroot.add_from_dir_tree(
                        source_path = path, dest_path = 'include', tracked = False,
                        follow_links = True)
                for path in lib_paths:
                    sysroot.add_from_dir_tree(
                        source_path = path, dest_path = 'lib', tracked = False,
                        follow_links = True)

    # Add package manifests to a combined manifest.
    def add_manifests(self, manifest):
        for package in self.packages.values():
            manifest.combine(package.manifest)

    # Get the binary path for a package.
    def _get_bin_path(self, package):
        return os.path.join(self.bin_dir, '%s.bin.tar.xz' % (package.full_name))
