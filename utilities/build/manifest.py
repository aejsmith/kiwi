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

from enum import Enum
from SCons.Script import *
import json
import os
import SCons
import stat

class ManifestEntryType(Enum):
    File = 1
    Link = 2

class ManifestEntry:
    def __init__(self, entry_type, target, checksum = None):
        self.entry_type = entry_type
        self.target = target
        self.checksum = checksum

    def serialise(self):
        result = {}
        result['type'] = self.entry_type.name
        if self.entry_type == ManifestEntryType.File:
            result['checksum'] = self.checksum
        else:
            result['target'] = str(self.target)
        return result

class Manifest:
    def __init__(self):
        self.entries = {}
        self.dependencies = []
        self.finalised = False

    # Add a file to the manifest. The untracked flag means that the file will
    # not be added as a dependency when the manifest is built.
    def add_file(self, path, target, untracked = False):
        if type(target) != SCons.Node.FS.File:
            raise Exception('Must be a SCons.Node.FS.File instance')

        self._add_entry(path, ManifestEntry(ManifestEntryType.File, target))

        if not untracked:
            # Keep dependencies ordered by the insertion order. Some of the
            # userspace build relies on the core libraries being built first.
            self.dependencies.append(target)

    # Add a symbolic link to the manifest.
    def add_link(self, path, target):
        self._add_entry(path, ManifestEntry(ManifestEntryType.Link, target))

    def _add_entry(self, path, entry):
        if self.finalised:
            raise Exception('Manifest is already finalised')

        # Store root-relative normalized paths.
        path = os.path.normpath(path).lstrip('/')

        if path in self.entries:
            raise Exception("Adding duplicate path '%s' to manifest" % (path))

        self.entries[path] = entry

    # Add an existing directory tree to the manifest.
    def add_from_dir_tree(self, env, source_path, dest_path = ''):
        try:
            entries = os.listdir(source_path)
        except:
            return

        entries.sort()

        for entry in entries:
            entry_source_path = os.path.join(source_path, entry)
            entry_dest_path = os.path.join(dest_path, entry)
            try:
                st = os.lstat(entry_source_path)

                if stat.S_ISDIR(st.st_mode):
                    self.add_from_dir_tree(env, entry_source_path, entry_dest_path)
                elif stat.S_ISLNK(st.st_mode):
                    link = os.readlink(entry_source_path)
                    self.add_link(entry_dest_path, link)
                elif stat.S_ISREG(st.st_mode):
                    self.add_file(entry_dest_path, env.File(entry_source_path))
            except OSError:
                continue

    # Get checksums for all files.
    def finalise(self):
        if self.finalised:
            return

        for entry in self.entries.values():
            if entry.entry_type == ManifestEntryType.File and entry.checksum == None:
                # Use the signature SCons has so we don't unnecessarily
                # recalculate it for files that we are building. This seems to
                # be public API as it is used in the example Decider function
                # in the man page.
                entry.checksum = entry.target.get_csig()

        self.finalised = True

    # Get a sorted list of directories needed for the manifest's contents.
    def get_dirs(self):
        dirs = set()
        for path in self.entries.keys():
            while True:
                path = os.path.dirname(path)
                if path != '':
                    dirs.add(path)
                else:
                    break
        dirs = list(dirs)
        dirs.sort()
        return dirs

    # Serialise the manifest to a file.
    def serialise(self, dest_path):
        if not self.finalised:
            raise Exception('Manifest is not finalised')

        result = {}
        for (path, entry) in self.entries.items():
            result[path] = entry.serialise()

        # Sort so we have stable output.
        with open(dest_path, 'w') as dest_file:
            dest_file.write(json.dumps(result, sort_keys = True, indent = 4))

def manifest_action(target, source, env):
    manifest = env['MANIFEST']
    manifest.finalise()
    manifest.serialise(str(target[0]))
    return 0
def manifest_method(env, target):
    manifest = env['MANIFEST']

    # Depend on the tracked files in the manifest. This causes everything it
    # refers to to be built.
    result = env.Command(target, manifest.dependencies, Action(manifest_action, '$GENCOMSTR'))

    # We always write the manifest, so that:
    #  1. We update untracked files, which aren't dependencies.
    #  2. Link changes get written, we can't have any dependencies for these.
    # Users of the manifest will only be rebuilt if it has actually changed.
    AlwaysBuild(result)

    return result

def add_file_method(env, target, path):
    env['MANIFEST'].add_file(path, target)

def add_link_method(env, target, path):
    env['MANIFEST'].add_link(path, target)
