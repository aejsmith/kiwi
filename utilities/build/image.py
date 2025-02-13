#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

from manifest import *
from SCons.Script import *
import io
import os
import shutil
import tarfile
import tempfile
import time
import uuid

def run_command(command):
    if ARGUMENTS.get('V') == '1':
        print(command)
        verbose = ''
    else:
        verbose = ' >> /dev/null 2>&1'
    ret = os.system('%s%s' % (command, verbose))
    if ret != 0:
        raise Exception('Command failed (%d): %s' % (ret, command))

class TARArchive:
    def __init__(self, path):
        self.tar = tarfile.open(path, 'w')

    def finish(self):
        self.tar.close()

    def make_dir(self, name):
        if len(name) == 0:
            return
        try:
            self.tar.getmember(name)
        except KeyError:
            self.make_dir(os.path.dirname(name))

            tarinfo = tarfile.TarInfo(name)
            tarinfo.type  = tarfile.DIRTYPE
            tarinfo.mtime = int(time.time())
            tarinfo.mode  = 0o755
            tarinfo.uid   = 0
            tarinfo.gid   = 0
            tarinfo.uname = "root"
            tarinfo.gname = "root"

            self.tar.addfile(tarinfo)

    def make_file(self, name, data):
        self.make_dir(os.path.dirname(name))

        file = io.BytesIO(data.encode('utf-8'))

        tarinfo = tarfile.TarInfo(name)
        tarinfo.type     = tarfile.REGTYPE
        tarinfo.mtime    = int(time.time())
        tarinfo.mode     = 0o644
        tarinfo.uid      = 0
        tarinfo.gid      = 0
        tarinfo.uname    = "root"
        tarinfo.gname    = "root"
        tarinfo.size     = len(data)

        self.tar.addfile(tarinfo, file)

    def add_file(self, path, target):
        path = os.path.normpath(path).lstrip('/')

        self.make_dir(os.path.dirname(path))

        with open(str(target), 'rb') as file:
            tarinfo = self.tar.gettarinfo(None, path, file)
            tarinfo.uid   = 0
            tarinfo.gid   = 0
            tarinfo.uname = "root"
            tarinfo.gname = "root"

            self.tar.addfile(tarinfo, file)

    def add_files(self, files):
        for (path, target) in files:
            self.add_file(path, target)

    def add_link(self, path, target):
        path = os.path.normpath(path).lstrip('/')

        self.make_dir(os.path.dirname(path))

        tarinfo = tarfile.TarInfo(path)
        tarinfo.type     = tarfile.SYMTYPE
        tarinfo.linkname = target
        tarinfo.mtime    = int(time.time())
        tarinfo.mode     = 0o644
        tarinfo.uid      = 0
        tarinfo.gid      = 0
        tarinfo.uname    = "root"
        tarinfo.gname    = "root"

        self.tar.addfile(tarinfo)

    def add_links(self, links):
        for (path, target) in links:
            self.add_link(path, target)

def fs_archive_action(target, source, env):
    manifest = env['MANIFEST']

    tar = TARArchive(str(target[0]))

    for (path, entry) in manifest.entries.items():
        if entry.entry_type == ManifestEntryType.File:
            tar.add_file(path, entry.target)
        elif entry.entry_type == ManifestEntryType.Link:
            tar.add_link(path, entry.target)

    tar.finish()
    return 0

# Create a TAR archive containing the contents of the manifest.
def fs_archive_method(env, target):
    # Build a manifest file.
    manifest_path = '%s.manifest' % (str(target))
    manifest_file = env.Manifest(manifest_path)

    # Depend on the manifest file, which builds the FS content. The manifest
    # will change when any of the content changes and cause the archive to
    # rebuild.
    return env.Command(target, [manifest_file], Action(fs_archive_action, '$GENCOMSTR'))

def boot_archive_action(target, source, env):
    config = env['CONFIG']

    tar = TARArchive(str(target[0]))

    files = [
        ('kernel', env['KERNEL']),
        ('modules/fs.tar', env['FS_ARCHIVE']),
    ]

    for mod in env['MODULES']:
        files += [('modules/' + os.path.basename(str(mod)), mod)]

    tar.add_files(files)

    kboot_cfg = 'kboot "/kernel" "/modules"\n'
    tar.make_file('kboot.cfg', kboot_cfg)

    tar.finish()
    return 0

# Create a boot archive.
def boot_archive_method(env, target):
    dependencies = [env['KERNEL']] + env['MODULES'] + env['KBOOT'] + [env['FS_ARCHIVE']]
    return env.Command(target, dependencies, Action(boot_archive_action, '$GENCOMSTR'))

def iso_image_action(target, source, env):
    config = env['CONFIG']
    fs_archive = str(env['FS_ARCHIVE'])
    kernel = str(env['KERNEL'])

    # Create a temporary work directory.
    tmp_dir = tempfile.mkdtemp('.kiwiiso')
    try:
        os.makedirs(os.path.join(tmp_dir, 'boot'))
        os.makedirs(os.path.join(tmp_dir, 'kiwi', 'modules'))

        # Copy stuff into it.
        shutil.copy(kernel, os.path.join(tmp_dir, 'kiwi'))
        shutil.copy(fs_archive, os.path.join(tmp_dir, 'kiwi', 'modules'))
        for mod in env['MODULES']:
            shutil.copy(str(mod), os.path.join(tmp_dir, 'kiwi', 'modules'))

        # Write the KBoot configuration.
        with open(os.path.join(tmp_dir, 'boot', 'kboot.cfg'), 'w') as kboot_file:
            kboot_file.write('set "timeout" 5\n')
            kboot_file.write('entry "Kiwi" {\n')
            if len(config['FORCE_VIDEO_MODE']) > 0:
                kboot_file.write('   set "video_mode" "%s"\n' % (config['FORCE_VIDEO_MODE']))
            kboot_file.write('   kboot "/kiwi/kernel" "/kiwi/modules"\n')
            kboot_file.write('}\n')

        # Create the ISO.
        run_command(
            '%s --bin-dir=build/%s-%s/boot/bin --targets="%s" --label="Kiwi CDROM" %s %s' %
                (env['KBOOT_MKISO'], config['ARCH'], config['BUILD'], config['KBOOT_TARGETS'], target[0], tmp_dir))
    finally:
        shutil.rmtree(tmp_dir)

    return 0

# Function to generate an ISO image.
def iso_image_method(env, target):
    dependencies = [env['KERNEL']] + env['MODULES'] + env['KBOOT'] + [env['FS_ARCHIVE']] + [env['KBOOT_MKISO']]
    return env.Command(target, dependencies, Action(iso_image_action, '$GENCOMSTR'))

# IMAGE_SIZE is the desired combined image size, the system image size accounts
# for the MBR and partition alignment by image_tool.
IMAGE_SIZE = 2 * 1024 * 1024 * 1024
EFI_IMAGE_SIZE = 10 * 1024 * 1024
SYSTEM_IMAGE_SIZE = IMAGE_SIZE - EFI_IMAGE_SIZE - 4096

def escape(s):
    return s.replace('"', '\\"')

def do_disk_image_parts(target, source, env, persistent):
    config = env['CONFIG']
    manifest = env['MANIFEST']

    parts_path = str(target[0])
    system_path = parts_path[:-6] + '.system'
    uuid_path = parts_path[:-6] + '.uuid'
    efi_path = parts_path[:-6] + '.efi'
    manifest_path = parts_path[:-6] + '.manifest'

    system_uuid = ''
    create = True
    if persistent:
        # Check if the file already exists.
        if os.path.isfile(system_path):
            # We should not recreate it.
            create = False

            # We should have a file containing the filesystem UUID.
            try:
                with open(uuid_path, 'r') as uuid_file:
                    system_uuid = uuid_file.readlines()[0].strip()
            except:
                pass
            if not system_uuid:
                # If we can't get the UUID, we cannot update it. Fail instead
                # of just blowing away the image.
                raise Exception('Cannot get image UUID, will not update')
    if create:
        # Make a UUID.
        system_uuid = uuid.uuid1()
        if persistent:
            with open(uuid_path, 'w') as uuid_file:
                uuid_file.write('%s\n' % (system_uuid))

    # Create a temporary work directory.
    tmp_dir = tempfile.mkdtemp('.kiwiimg')
    try:
        # If updating the image, fetch its current manifest.
        orig_manifest = None
        if not create:
            orig_manifest_path = os.path.join(tmp_dir, 'orig.manifest')
            debugfs_path = os.path.join(tmp_dir, 'debugfs_manifest.txt')
            with open(debugfs_path, 'w') as debugfs_file:
                debugfs_file.write('dump ".manifest" "%s"\n' % (escape(orig_manifest_path)))

            run_command('debugfs -w -f "%s" "%s"' % (debugfs_path, system_path))

            orig_manifest = Manifest()
            orig_manifest.deserialise(orig_manifest_path)

            if ARGUMENTS.get('MANIFEST_DIFF') == '1':
                manifest.print_diff(orig_manifest)

        # Get the manifest actions to apply.
        actions = manifest.get_actions(orig_manifest)

        # Write a KBoot configuration for the system image.
        kboot_system_path = os.path.join(tmp_dir, 'kboot_system.cfg')
        with open(kboot_system_path, 'w') as kboot_file:
            kboot_file.write('set "timeout" 5\n')
            kboot_file.write('entry "Kiwi" {\n')
            if len(config['FORCE_VIDEO_MODE']) > 0:
                kboot_file.write('   set "video_mode" "%s"\n' % (config['FORCE_VIDEO_MODE']))
            kboot_file.write('   kboot "/system/kernel/kernel" "/system/kernel/modules"\n')
            kboot_file.write('}\n')

        # Write a KBoot configuration for the EFI image.
        kboot_efi_path = os.path.join(tmp_dir, 'kboot_efi.cfg')
        with open(kboot_efi_path, 'w') as kboot_file:
            kboot_file.write('device "uuid:%s"\n' % (system_uuid))
            kboot_file.write('config "/system/boot/kboot.cfg"\n')

        # Write a debugfs command file to write the system image contents.
        debugfs_path = os.path.join(tmp_dir, 'debugfs.txt')
        with open(debugfs_path, 'w') as debugfs_file:
            # We first need to create the directory hierarchy for the manifest.
            for dir in actions.dirs:
                debugfs_file.write('mkdir "%s"\n' % (escape(dir)))

            # Remove removed/changed files.
            for remove in actions.removes:
                debugfs_file.write('rm "%s"\n' % (escape(remove)))

            # Write files/links.
            for (path, target) in actions.files:
                debugfs_file.write('write "%s" "%s"\n' % (escape(str(target)), escape(path)))
            for (path, target) in actions.links:
                debugfs_file.write('symlink "%s" "%s"\n' % (escape(path), escape(target)))

            # Write KBoot config. Binaries are already in the manifest.
            debugfs_file.write('rm "system/boot/kboot.cfg"\n')
            debugfs_file.write('write "%s" "system/boot/kboot.cfg"\n' % (escape(kboot_system_path)))

            # Write the manifest.
            debugfs_file.write('rm ".manifest"\n')
            debugfs_file.write('write "%s" ".manifest"\n' % (escape(manifest_path)))

        # If creating, create and format new system image file.
        if create:
            with open(system_path, 'wb') as system_file:
                system_file.truncate(SYSTEM_IMAGE_SIZE)

            run_command('mke2fs -t ext2 -L Kiwi -U %s -F "%s"' % (system_uuid, system_path))

        # Write the contents.
        run_command('debugfs -w -f "%s" "%s"' % (debugfs_path, system_path))

        # Install the boot sector.
        if config['ARCH'] == 'amd64':
            run_command(
                '%s --bin-dir=build/%s-%s/boot/bin --target=bios --image="%s" --offset=0 --path=system/boot/kboot.bin' %
                    (env['KBOOT_INSTALL'], config['ARCH'], config['BUILD'], system_path))

        # Create new empty EFI image file.
        with open(efi_path, 'wb') as efi_file:
            efi_file.truncate(EFI_IMAGE_SIZE)

        # Format the EFI image.
        run_command('mkdosfs "%s"' % (efi_path))

        # Add its contents (config and all EFI binaries).
        run_command('mmd -i "%s" ::/EFI' % (efi_path))
        run_command('mmd -i "%s" ::/EFI/BOOT' % (efi_path))
        run_command('mcopy -i "%s" "%s" ::/EFI/BOOT/kboot.cfg' % (efi_path, kboot_efi_path))
        for file in env['KBOOT']:
            name = file.name.lower()
            if name.endswith('.efi'):
                # Need to rename 'kboot<arch.efi>' to 'boot<arch>.efi' to be in
                # the default boot location.
                name = name.replace('kboot', 'boot')
                run_command('mcopy -i "%s" "%s" "::/EFI/BOOT/%s"' % (efi_path, str(file), name))
    finally:
        shutil.rmtree(tmp_dir)

    # Write our output parts file with the source checksums.
    with open(parts_path, 'w') as parts_file:
        for f in source:
            parts_file.write('%s\n' % (f.get_csig()))

    return 0

def disk_image_parts_action(target, source, env, func = do_disk_image_parts):
    # There doesn't seem to be a way to pass an argument to an Action function
    # in SCons so we have to use separate functions for persistent/non-
    # persistent. However, on its own, that means SCons won't rebuild when
    # do_disk_image_parts changes. To work around this, we pass it in a default
    # argument. SCons includes default arguments in the checksum for a function.
    return func(target, source, env, False)

def disk_image_parts_persistent_action(target, source, env, func = do_disk_image_parts):
    return func(target, source, env, True)

def do_disk_image(target, source, env):
    image_tool = str(env['IMAGE_TOOL'])
    image_path = str(target[0])
    efi_path = image_path + '.efi'
    system_path = image_path + '.system'
    run_command('%s %s %s %s' % (image_tool, image_path, efi_path, system_path))
    return 0

def disk_image_action(target, source, env, func = do_disk_image):
    return func(target, source, env)

def disk_image_persistent_action(target, source, env, func = do_disk_image):
    # Warn about this. Users should not expect their changes to the combined
    # image to persist.
    print('Warning: Changes to user.img will NOT be persistent. Instead, changes to')
    print('user.img.system will be mirrored to user.img each time it is built.')
    return func(target, source, env)

# Function to generate a disk image.
def disk_image_method(env, target, persistent = False):
    image_path = str(target)

    # Build a manifest file.
    manifest_path = '%s.manifest' % (image_path)
    manifest_file = env.Manifest(manifest_path)

    # Build the parts of the image (system + EFI).
    #
    # The main output file of this is a dummy file, named '<image>.parts'. The
    # build function for this generates '<image>.system' and '<image>.efi', but
    # does not declare them to SCons. The declared output file only contains
    # the checksums of the source dependencies, which means that anything that
    # depends on the output will rebuild when the source dependencies change.
    #
    # This is done as an optimisation to avoid SCons checksumming the image
    # parts. In addition, the QEMU target depends on the '.parts' file, and
    # runs with the separate parts, rather than using the combined output file.
    # This means that for the usual build and run iteration cycle, we don't
    # have checksumming of the disk images and generation of the combined image,
    # which greatly reduces the build time.
    #
    # The other reason we generate the image in parts is that debugfs does not
    # support filesystems at an offset in the file. Therefore, we cannot use it
    # to update a combined image.
    #
    # Here we depend on the manifest file, which builds the image content. This
    # will change when any of the content changes and cause the image to
    # rebuild. Also depend on KBoot binaries and installer.
    parts_path = '%s.parts' % (image_path)
    parts_dependencies = [manifest_file, env['KBOOT_INSTALL']] + env['KBOOT']
    parts_action = disk_image_parts_persistent_action if persistent else disk_image_parts_action
    parts_file = env.Command(parts_path, parts_dependencies, Action(parts_action, '$GENCOMSTR'))

    # Build the combined image.
    image_dependencies = [parts_file, env['IMAGE_TOOL']]
    image_action = disk_image_persistent_action if persistent else disk_image_action
    image_file = env.Command(image_path, image_dependencies, Action(image_action, '$GENCOMSTR'))

    # For now, we set the combined image as AlwaysBuild. We definitely want this
    # for the persistent user image, as we want user changes to the parts to be
    # reflected to the combined image every time it is requested. However we
    # also do it on the clean image, as a safeguard because of the hack to avoid
    # checksumming the parts files. It may be OK without this, but I'm erring
    # on the side of caution.
    AlwaysBuild(image_file)

    return image_file
