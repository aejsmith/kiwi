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
import copy
import hashlib
import json
import os
import SCons
import stat

class ManifestEntryType(Enum):
    File = 1
    Link = 2

class ManifestEntry:
    def __init__(self, entry_type = None, target = None, checksum = None):
        self.entry_type = entry_type
        self.target = target
        self.checksum = checksum

    def serialise(self):
        result = {}
        result['type'] = self.entry_type.name
        if self.entry_type == ManifestEntryType.File:
            result['checksum'] = self.checksum
        elif self.entry_type == ManifestEntryType.Link:
            result['target'] = str(self.target)
        return result

    def deserialise(self, values):
        self.entry_type = ManifestEntryType[values['type']]
        if self.entry_type == ManifestEntryType.File:
            self.checksum = str(values['checksum'])
        elif self.entry_type == ManifestEntryType.Link:
            self.target = str(values['target'])

    def compare(self, other):
        if self.entry_type == other.entry_type:
            if self.entry_type == ManifestEntryType.File:
                return self.checksum == other.checksum
            elif self.entry_type == ManifestEntryType.Link:
                return self.target == other.target
        return False

# List of actions to apply a manifest to an image, optionally from a current
# manifest.
class ManifestActions:
    # Apply actions in this order:
    dirs    = []    # 1. (path) Create directories in the order specified.
    removes = []    # 2. (path) Remove existing files/links.
    files   = []    # 3. (path, target) Write files.
    links   = []    # 4. (path, target) Create links.

class Manifest:
    def __init__(self):
        self.entries = {}
        self.dependencies = []
        self.finalised = False

    # Add a file to the manifest.
    #
    # If tracked is True, the given target must be a SCons File instance, and
    # the file will be added as a dependency when the manifest is built.
    # 
    # If tracked is False, the target can be a plain path string, and the file
    # will not be added as a dependency when the manifest is built. This can be
    # used when we are guaranteeing that the file exists by some other way, to
    # avoid declaring files to SCons and therefore avoiding overhead from its
    # checksumming and other tracking. The package system uses this.
    def add_file(self, path, target, tracked = True):
        if not isinstance(target, SCons.Node.FS.File) and (tracked or not isinstance(target, str)):
            raise Exception('Target is not valid')

        self._add_entry(path, ManifestEntry(ManifestEntryType.File, target))

        if tracked:
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

    # Add an existing directory tree to the manifest. The paths within the
    # manifest will be set as the path relative to source_path.
    #
    # If tracked is True, files will be tracked as per add_file(). env must be
    # a valid SCons environment.
    #
    # If tracked is False, env need not be specified.
    def add_from_dir_tree(self, source_path, env = None, tracked = True, dest_path = ''):
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
                    self.add_from_dir_tree(entry_source_path, env, tracked, entry_dest_path)
                elif stat.S_ISLNK(st.st_mode):
                    link = os.readlink(entry_source_path)
                    self.add_link(entry_dest_path, link)
                elif stat.S_ISREG(st.st_mode):
                    if tracked:
                        self.add_file(entry_dest_path, env.File(entry_source_path), tracked = True)
                    else:
                        self.add_file(entry_dest_path, entry_source_path, tracked = False)
            except OSError:
                continue

    def _checksum_file(self, path):
        hash = hashlib.md5()
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash.update(chunk)
        return hash.hexdigest()

    # Add another manifest's contents into this manifest.
    def combine(self, other):
        if self.finalised:
            raise Exception('Manifest is already finalised')
        elif not other.finalised:
            raise Exception('Source manifest is not finalised')

        for (path, entry) in other.entries.items():
            if path in self.entries:
                raise Exception("Duplicate manifest entries for '%s' when combining" % (path))

            self.entries[path] = copy.copy(entry)

            if entry.target in other.dependencies:
                self.dependencies.append(entry.target)

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
                if isinstance(entry.target, SCons.Node.FS.File):
                    entry.checksum = entry.target.get_csig()
                else:
                    entry.checksum = self._checksum_file(entry.target)

        self.finalised = True

    def _get_dirs(self):
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

    # Get a set of actions needed to apply the manifest. Optionally, this takes
    # an original manifest, in which case the returned set of actions will apply
    # the difference between the original and new manifest.
    def get_actions(self, orig_manifest = None):
        actions = ManifestActions()

        # Create all directories for files in the new manifest. We don't
        # currently remove any directories that are no longer needed in the old
        # manifest.
        actions.dirs = self._get_dirs()

        # Generate remove actions based on the original manifest. We explicitly
        # remove changed files, since the ext2 image write needs this (debugfs
        # does not handle overwriting).
        if orig_manifest != None:
            for (path, orig_entry) in orig_manifest.entries.items():
                if path in self.entries:
                    entry = self.entries[path]
                    if not entry.compare(orig_entry):
                        # Changed, remove it.
                        actions.removes.append(path)
                else:
                    # Not in new manifest, remove it.
                    actions.removes.append(path)

        for (path, entry) in self.entries.items():
            # Don't do anything if hasn't changed from the previous manifest.
            changed = True
            if orig_manifest != None and path in orig_manifest.entries:
                changed = not entry.compare(orig_manifest.entries[path])

            if changed:
                if entry.entry_type == ManifestEntryType.File:
                    actions.files.append((path, entry.target))
                elif entry.entry_type == ManifestEntryType.Link:
                    actions.links.append((path, entry.target))
                else:
                    raise Exception("Unhandled ManifestEntryType")

        return actions

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

    # Deserialise from a file.
    #
    # Optionally, the manifest can be loaded with a root path, in which case
    # all files loaded will have their target set as an untracked file relative
    # to that root path.
    # 
    # If no root is specified, files will not have targets, and therefore the
    # manifest will only be useful for comparison.
    def deserialise(self, source_path, root_path = None):
        if self.entries or self.finalised:
            raise Exception('Deserialsing to non-empty manifest')

        result = {}
        with open(source_path, 'r') as source_file:
            result = json.loads(source_file.read())

        for (path, values) in result.items():
            entry = ManifestEntry()
            entry.deserialise(values)

            if root_path and entry.entry_type == ManifestEntryType.File:
                entry.target = os.path.join(root_path, path)

            self.entries[path] = entry

        self.finalised = True

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
